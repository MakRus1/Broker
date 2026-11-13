#include <components/inquiries.hpp>
#include <components/shipment_route.hpp>
#include <converters/shipment_command.hpp>
#include <converters/transport_location.hpp>
#include <events/shipment_command.hpp>
#include <helpers/settings.hpp>
#include <models/errors.hpp>
#include <models/number_allocation.hpp>
#include <platform/document_number.hpp>
#include <platform/http_client.hpp>
#include <platform/outbox/component.hpp>
#include <sales/sql_queries.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/utils/text_light.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace sales {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

InquiriesComponent::InquiriesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()),
      counterparties_(context.FindComponent<clients::counterparties::Component>().GetClient()),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()),
      transport_locations_(
          context.FindComponent<clients::transport_locations::Component>().GetClient()) {}

userver::yaml_config::Schema InquiriesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::Inquiry InquiriesComponent::SaveInquiry(const std::string& organization,
    const platform::auth::Identity& identity, const models::SaveInquiryRequest& request) const {
    const auto& value = request;

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto previous = transaction.Execute(sql::kLockInquiries, organization, value.id);
    if (previous.IsEmpty()) {
        if (value.status != models::InquiryStatus::Draft)
            throw ApiError<400>{"invalid_status", "Новый запрос должен быть черновиком"};
    } else {
        const auto old = models::ParseModelJson(
            previous.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
        if (old.converting || old.shipment_id.has_value())
            throw ApiError<409>{"conversion_started", "Запрос уже передан в создание поставки"};
        if (old.status == models::InquiryStatus::Agreed && value.status == old.status)
            throw ApiError<409>{"agreed_request", "Верните запрос в работу перед изменением"};
    }
    if (value.ready_date.has_value() && value.desired_delivery_date.has_value() &&
        value.desired_delivery_date < value.ready_date)
        throw ApiError<400>{"invalid_dates", "Дата доставки не может быть раньше готовности груза"};
    if (value.status == models::InquiryStatus::Agreed &&
        (!value.cargo_description.has_value() ||
            userver::utils::text::Trim(value.cargo_description.value()).empty()))
        throw ApiError<400>{"cargo_required", "Перед согласованием опишите груз"};
    std::string number;
    if (value.expected_revision == 0) {
        const auto settings =
            helpers::ReadSalesSettings(transaction.Execute(sql::kGetSettings, organization));
        const auto allocation = transaction
                                    .Execute(sql::kNextDocumentNumber, organization,
                                        std::string{"inquiry"}, settings.reset_number_yearly)
                                    .AsSingleRow<models::NumberAllocation>(pg::kRowTag);
        number = platform::FormatDocumentNumber(
            settings.inquiry_number_template, allocation.Date, allocation.Sequence);
        if (number.size() > 128)
            throw ApiError<400>{"number_too_long", "Номер запроса длиннее 128 символов"};
    }

    const auto document = json::ValueBuilder(value).ExtractValue();
    const auto changed = value.expected_revision == 0
        ? transaction.Execute(sql::kCreateInquiries, organization, document, number)
        : transaction.Execute(
              sql::kUpdateInquiries, organization, document, value.expected_revision);
    if (changed.IsEmpty())
        throw ApiError<409>{
            "revision_conflict", "Запись уже существует, изменена или передана в поставку"};
    const auto rows = transaction.Execute(sql::kGetInquiries, organization, value.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    const auto result = models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});

    transaction.Execute(sql::kWriteHistory, organization, std::string{"inquiries"}, result.id,
        result.revision, identity.ActorId(), json::ValueBuilder(result).ExtractValue());

    transaction.Commit();

    return result;
}

models::Inquiry InquiriesComponent::FindInquiry(
    const std::string& organization, const std::string& id) const {
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetInquiries, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    return models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
}

std::vector<models::InquiryHistoryEntry> InquiriesComponent::GetInquiryHistory(
    const std::string& organization, const models::HistoryRequest& request) const {
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kHistoryInquiries,
        organization, request.id, request.after_revision.value_or(0));
    std::vector<models::InquiryHistoryEntry> response;
    response.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<json::Value>())
        response.push_back(
            models::ParseModelJson(row, std::type_identity<models::InquiryHistoryEntry>{}));

    return response;
}

models::ShipmentCreationContext InquiriesComponent::PrepareShipmentCreation(
    const std::string& organization, std::string_view token, const models::Inquiry& inquiry) const
    try {
    const auto party =
        counterparties_.ApiV1CounterpartiesGet({.body = {.id = inquiry.customer_id}}, token)
            .counterparty;
    if (party.archived)
        throw ApiError<409>{"archived_customer", "Клиент находится в архиве"};
    models::ShipmentCreationContext creation{.InquiryRevision = inquiry.revision,
        .Logist = party.logist.value_or(std::nullopt),
        .RouteSegments = inquiry.route_segments};
    if (creation.Logist) {
        const auto employees = organizations_.ApiOrganizationsEmployeesList(
            {.body = {.limit = 1, .organization_id = organization, .subject = creation.Logist}},
            token);
        if (employees.items.empty())
            throw ApiError<400>{
                "invalid_logist", "В карточке клиента назначен недействующий логист"};
    }

    const auto references = route_rules::CollectReferences(creation.RouteSegments);
    models::TransportLocations locations;
    if (!references.LocationIds.empty()) {
        const auto response = transport_locations_.ApiV1RouteLocationsGetBatch(
            {.body = {.ids = {references.LocationIds.begin(), references.LocationIds.end()}}},
            token);
        for (const auto& location : response.items)
            locations.emplace(location.id, converters::ToTransportLocation(location));
    }
    route_rules::ResolveLocations(creation.RouteSegments, locations);
    for (const auto& id : references.CounterpartyIds) {
        const auto counterparty =
            counterparties_.ApiV1CounterpartiesGet({.body = {.id = id}}, token).counterparty;
        if (counterparty.archived)
            throw ApiError<400>{
                "invalid_route_counterparty", "Выберите действующего контрагента участка"};
    }

    return creation;
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к связанным данным"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить связанные данные"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
}

models::Inquiry InquiriesComponent::StartShipmentCreation(const std::string& organization,
    const platform::auth::Identity& actor, const models::CreateShipmentRequest& request,
    const models::ShipmentCreationContext& creation) const {

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto locked = transaction.Execute(sql::kLockInquiries, organization, request.id);
    if (locked.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    const auto inquiry = models::ParseModelJson(
        locked.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
    if (inquiry.shipment_id.has_value() || inquiry.converting) {
        transaction.Commit();
        return inquiry;
    }
    if (inquiry.revision != request.expected_revision ||
        inquiry.revision != creation.InquiryRevision)
        throw ApiError<409>{"revision_conflict", "Запрос изменён. Обновите карточку"};
    if (inquiry.status != models::InquiryStatus::Agreed || inquiry.conversion_error.has_value())
        throw ApiError<409>{"not_agreed", "Сначала согласуйте запрос"};
    const auto command = converters::ToShipmentCommand(organization, actor, inquiry, creation);

    const auto document = json::ValueBuilder(command).ExtractValue();
    transaction.Execute(sql::kStartConversion, organization, inquiry.id, document);
    platform::outbox::Enqueue(transaction, kCreateShipment, inquiry.id, command);
    const auto rows = transaction.Execute(sql::kGetInquiries, organization, inquiry.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    const auto result = models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});

    transaction.Commit();

    return result;
}

}  // namespace sales
