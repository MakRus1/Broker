#include <components/inquiries.hpp>
#include <converters/shipment_command.hpp>
#include <events/shipment_command.hpp>
#include <helpers/settings.hpp>
#include <models/errors.hpp>
#include <models/number_allocation.hpp>
#include <platform/document_number.hpp>
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
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()) {}

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
