#include <components/inquiries.hpp>
#include <events/shipment_command.hpp>
#include <helpers/references.hpp>
#include <helpers/settings.hpp>
#include <map>
#include <platform/document_number.hpp>
#include <platform/outbox/component.hpp>
#include <registry/fields.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace sales {
using namespace helpers;

InquiriesComponent::InquiriesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      deps_{
          .counterparties = context.FindComponent<clients::counterparties::Component>().GetClient(),
          .organizations = context.FindComponent<clients::organizations::Component>().GetClient(),
          .transport_locations =
              context.FindComponent<clients::transport_locations::Component>().GetClient(),
          .pg_cluster =
              context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()} {
}

userver::yaml_config::Schema InquiriesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

namespace {
struct NumberAllocation {
    userver::utils::datetime::Date Date;
    std::int64_t Sequence;
};
}  // namespace

models::Inquiry InquiriesComponent::Save(const models::SaveInquiryRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto& value = request;
    ValidateReferences(deps, context, value.customer_id, value.manager_subject);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
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
        const auto settings = Settings(transaction.Execute(sql::kGetSettings, organization));
        const auto allocation = transaction
                                    .Execute(sql::kNextDocumentNumber, organization,
                                        std::string{"inquiry"}, settings.reset_number_yearly)
                                    .AsSingleRow<NumberAllocation>(pg::kRowTag);
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
        result.revision, platform::auth::GetIdentity(context).ActorId(),
        json::ValueBuilder(result).ExtractValue());
    transaction.Commit();
    return result;
}

models::Inquiry InquiriesComponent::Get(
    const models::IdRequest& request, userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kGetInquiries,
        platform::auth::GetOrganization(platform::auth::GetIdentity(context)), request.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    return models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
}

InquiriesComponent::ListResult InquiriesComponent::List(const models::ListSalesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    try {
        const auto& body = request;
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        const auto scope = registry::CursorScope(body, organization);
        const bool date_sort =
            registry::RegistryOrderBy(kInquiriesRegistry.Fields, body.sort).CursorType ==
            registry::CursorKeyType::Date;
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto query = registry::BuildPage(kInquiriesRegistry, body, organization);
        auto rows = deps.pg_cluster
                        ->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                        .AsContainer<std::vector<RegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        ListResult response;
        response.items.reserve(rows.size());
        for (const auto& row : rows)
            response.items.push_back(
                models::ParseModelJson(row.Document, std::type_identity<models::Inquiry>{}));
        if (more) {
            const auto& last = rows.back();
            response.next_cursor =
                models::SalesRegistryCursor{.date_value = date_sort ? last.DateKey : std::nullopt,
                    .id = response.items.back().id,
                    .scope = scope,
                    .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        return response;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

std::vector<models::InquiryHistoryEntry> InquiriesComponent::History(
    const models::HistoryRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kHistoryInquiries,
        platform::auth::GetOrganization(platform::auth::GetIdentity(context)), request.id,
        request.after_revision.value_or(0));
    std::vector<models::InquiryHistoryEntry> response;
    response.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<json::Value>())
        response.push_back(
            models::ParseModelJson(row, std::type_identity<models::InquiryHistoryEntry>{}));
    return response;
}

models::Inquiry InquiriesComponent::CreateShipment(const models::CreateShipmentRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    platform::auth::CheckPermission(context, "shipments.write");
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto& actor = platform::auth::GetIdentity(context);
    // Справочник читаем до транзакции: назначение логиста фиксируется при постановке.
    const auto before_rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetInquiries, organization, request.id);
    if (before_rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    const auto before = models::ParseModelJson(
        before_rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
    if (before.shipment_id.has_value() || before.converting)
        return before;
    const auto party = GetCounterparty(deps, context, before.customer_id);
    if (party.archived)
        throw ApiError<409>{"archived_customer", "Клиент находится в архиве"};
    auto segments = before.route_segments;
    ResolveShipmentReferences(context, segments, party.logist.value_or(std::nullopt));
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto locked = transaction.Execute(sql::kLockInquiries, organization, request.id);
    if (locked.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    const auto inquiry = models::ParseModelJson(
        locked.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
    if (inquiry.shipment_id.has_value() || inquiry.converting) {
        transaction.Commit();
        return inquiry;
    }
    if (inquiry.revision != request.expected_revision || inquiry.revision != before.revision)
        throw ApiError<409>{"revision_conflict", "Запрос изменён. Обновите карточку"};
    if (inquiry.status != models::InquiryStatus::Agreed || inquiry.conversion_error.has_value())
        throw ApiError<409>{"not_agreed", "Сначала согласуйте запрос"};
    namespace shipment_models = clients::shipments::models;
    constexpr auto kInquirySourceType = "sales.inquiry";
    shipment_models::CreateShipmentRequest shipment;
    shipment.customer_id = inquiry.customer_id;
    shipment.idempotency_key =
        std::string{kInquirySourceType} + ":" + inquiry.id + ":" + std::to_string(inquiry.revision);
    shipment.reference = inquiry.number;
    shipment.cargo_description = inquiry.cargo_description;
    shipment.carriage_terms = inquiry.carriage_terms;
    shipment.delivery_address = inquiry.delivery_address;
    shipment.delivery_city = inquiry.delivery_city;
    shipment.delivery_country = inquiry.delivery_country;
    shipment.incoterms = inquiry.incoterms;
    shipment.loading = inquiry.loading;
    shipment.logist = party.logist.value_or(std::nullopt);
    shipment.route_segments.emplace();
    for (const auto& segment : segments)
        shipment.route_segments->push_back(
            shipment_models::ParseModelJson(json::ValueBuilder(segment).ExtractValue(),
                std::type_identity<shipment_models::ShipmentRouteSegment>{}));
    shipment_models::EnsureShipmentRequest command;
    command.organization_id = organization;
    command.source.type = kInquirySourceType;
    command.source.id = inquiry.id;
    command.actor_subject = actor.Subject;
    command.actor_issuer = actor.Issuer;
    command.shipment = std::move(shipment);
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

InquiriesComponent::FilterValuesResult InquiriesComponent::FilterValues(
    const models::SalesFilterValuesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    try {
        FilterValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(kInquiriesRegistry.Fields);
        if (!request.field.has_value())
            return result;
        const auto query = registry::BuildFilterValues(kInquiriesRegistry, request,
            platform::auth::GetOrganization(platform::auth::GetIdentity(context)));
        const auto rows =
            deps.pg_cluster->Execute(
                               pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        for (std::size_t i = 0; i < rows.size() && i < static_cast<std::size_t>(request.limit); ++i)
            result.values.push_back({.label = rows[i].Label, .value = rows[i].Value});
        if (rows.size() > static_cast<std::size_t>(request.limit))
            result.next_after = result.values.back().value;
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

// Проверяем ссылки с правами автора до фиксации неизменяемой команды.
// Доставка затем использует служебный токен и не зависит от пользовательской сессии.
void InquiriesComponent::ResolveShipmentReferences(
    const userver::server::request::RequestContext& context,
    std::vector<models::ShipmentRouteSegment>& segments,
    const std::optional<std::string>& logist) const try {
    const auto& deps = deps_;
    const auto token = platform::auth::GetAccessToken(context);
    if (logist.has_value()) {
        const auto employees = deps.organizations.ApiOrganizationsEmployeesList(
            {.body = {.limit = 1,
                 .organization_id =
                     platform::auth::GetOrganization(platform::auth::GetIdentity(context)),
                 .subject = logist}},
            token);
        if (employees.items.empty())
            throw ApiError<400>{
                "invalid_logist", "В карточке клиента назначен недействующий логист"};
    }
    std::set<std::string> ids;
    std::set<std::string> parties;
    const auto addLocation = [&](const auto& value) {
        if (value)
            ids.insert(value->id);
    };
    const auto addParty = [&](const auto& value) {
        if (value)
            parties.insert(*value);
    };
    for (const auto& segment : segments) {
        if (segment.sea) {
            addLocation(segment.sea->pol);
            addLocation(segment.sea->pod);
            addParty(segment.sea->departure_terminal_id);
            addParty(segment.sea->arrival_terminal_id);
        }
        if (segment.air) {
            addLocation(segment.air->origin_airport);
            addLocation(segment.air->destination_airport);
            addParty(segment.air->carrier_id);
        }
    }
    std::map<std::string, models::TransportLocation> locations;
    if (!ids.empty()) {
        const auto response = deps.transport_locations.ApiV1RouteLocationsGetBatch(
            {.body = {.ids = std::vector<std::string>(ids.begin(), ids.end())}}, token);
        for (const auto& location : response.items)
            locations.emplace(location.id,
                models::ParseModelJson(json::ValueBuilder(location).ExtractValue(),
                    std::type_identity<models::TransportLocation>{}));
    }
    const auto resolve = [&](auto& value, models::TransportLocationKind kind) {
        if (!value)
            return;
        const auto found = locations.find(value->id);
        if (found == locations.end() || found->second.kind != kind)
            throw ApiError<400>{
                "invalid_route_location", "Выберите транспортный пункт из справочника"};
        value = found->second;
    };
    for (auto& segment : segments) {
        if (segment.sea) {
            resolve(segment.sea->pol, models::TransportLocationKind::SeaPort);
            resolve(segment.sea->pod, models::TransportLocationKind::SeaPort);
        }
        if (segment.air) {
            resolve(segment.air->origin_airport, models::TransportLocationKind::Airport);
            resolve(segment.air->destination_airport, models::TransportLocationKind::Airport);
        }
    }
    for (const auto& id : parties) {
        const auto party = GetCounterparty(deps, context, id);
        if (party.archived)
            throw ApiError<400>{
                "invalid_route_counterparty", "Выберите действующего контрагента участка"};
    }
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

}  // namespace sales
