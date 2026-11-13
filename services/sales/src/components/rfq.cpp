#include <algorithm>
#include <components/rfq.hpp>
#include <events/shipment_command.hpp>
#include <helpers/editable_inquiry.hpp>
#include <map>
#include <models/errors.hpp>
#include <platform/http_client.hpp>
#include <sales/sql_queries.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/utils/text_light.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace models = sales::models;
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;
using sales::ApiError;

models::Rfq Read(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        throw ApiError<404>{"rfq_not_found", "Запрос ставок не найден в организации"};
    return models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::Rfq>{});
}
}  // namespace

namespace sales {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

RfqComponent::RfqComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()),
      counterparties_(context.FindComponent<clients::counterparties::Component>().GetClient()),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()),
      finance_(context.FindComponent<clients::finance::Component>().GetClient()) {}

userver::yaml_config::Schema RfqComponent::GetStaticConfigSchema() {
    return userver::components::ComponentBase::GetStaticConfigSchema();
}

models::Rfq RfqComponent::FindRfq(const std::string& organization, const std::string& id) const {
    return Read(cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetRfq, organization, id));
}

std::vector<models::Rfq> RfqComponent::ListRfqs(
    const std::string& organization, const std::string& inquiry_id) const {
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kListRfqs, organization, inquiry_id);
    std::vector<models::Rfq> result;
    for (const auto& row : rows)
        result.push_back(
            models::ParseModelJson(row[0].As<json::Value>(), std::type_identity<models::Rfq>{}));

    return result;
}

std::vector<models::RfqHistoryEntry> RfqComponent::GetRfqHistory(
    const std::string& organization, const models::HistoryRequest& request) const {
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kHistoryRfqs,
        organization, request.id, request.after_revision.value_or(0));
    std::vector<models::RfqHistoryEntry> result;
    for (const auto& row : rows)
        result.push_back(models::ParseModelJson(
            row[0].As<json::Value>(), std::type_identity<models::RfqHistoryEntry>{}));

    return result;
}

void RfqComponent::PrepareRfq(const std::string& organization, std::string_view token,
    const models::Inquiry& inquiry, models::RfqInput& input) const try {
    const auto party =
        counterparties_.ApiV1CounterpartiesGet({.body = {.id = inquiry.customer_id}}, token)
            .counterparty;
    if (party.archived)
        throw ApiError<409>{"archived_customer", "Выберите действующего клиента"};
    const auto employees =
        organizations_.ApiOrganizationsEmployeesList({.body = {.limit = 1,
                                                          .organization_id = organization,
                                                          .subject = input.responsible_subject}},
            token);
    if (employees.items.empty())
        throw ApiError<400>{"invalid_manager", "Выберите действующего сотрудника организации"};

    std::map<std::string, std::string> suppliers;
    std::map<std::string, std::string> labels;
    for (auto& quote : input.quotes) {
        if (!suppliers.contains(quote.supplier_id)) {
            const auto supplier =
                counterparties_.ApiV1CounterpartiesGet({.body = {.id = quote.supplier_id}}, token)
                    .counterparty;
            if (supplier.archived)
                throw ApiError<400>{"archived_supplier", "Поставщик в архиве"};
            suppliers.emplace(quote.supplier_id, supplier.name);
        }
        quote.supplier_name = suppliers.at(quote.supplier_id);
        for (auto& line : quote.lines) {
            if (!labels.contains(line.label_id)) {
                const auto label =
                    finance_.ApiV1FinancialLabelsGet({.body = {.id = line.label_id}}, token).label;
                if (label.archived ||
                    label.direction == clients::finance::models::FinancialLabelDirection::Income)
                    throw ApiError<400>{"invalid_label", "Выберите метку для расхода"};
                labels.emplace(line.label_id, label.name);
            }
            line.label_name = labels.at(line.label_id);
        }
    }
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к справочникам котировок"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить справочники котировок"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис справочников недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса справочников"};
}

models::Rfq RfqComponent::SaveRfq(const std::string& organization,
    const platform::auth::Identity& identity, std::int64_t expected, models::RfqInput input) const {

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inquiry = helpers::LockEditableInquiry(transaction, organization, input.inquiry_id);
    const auto segment = std::find_if(inquiry.route_segments.begin(), inquiry.route_segments.end(),
        [&](const auto& value) { return value.id == input.route_segment_id; });
    if (segment == inquiry.route_segments.end())
        throw ApiError<400>{"wrong_segment", "Участок не принадлежит входящему запросу"};
    const auto rows = transaction.Execute(sql::kGetRfq, organization, input.id);
    std::optional<models::Rfq> previous;
    if (!rows.IsEmpty())
        previous = Read(rows);
    if ((previous ? previous->revision : 0) != expected ||
        (previous && previous->input.inquiry_id != input.inquiry_id))
        throw ApiError<409>{"revision_conflict", "Запрос ставок изменён. Откройте его заново"};
    if (!previous &&
        transaction.Execute(sql::kListRfqs, organization, input.inquiry_id).Size() >= 100)
        throw ApiError<409>{"rfq_limit", "Во входящем запросе уже 100 запросов ставок"};
    models::Rfq value{.input = std::move(input),
        .inquiry_number = inquiry.number,
        .revision = expected + 1,
        .route_segment = *segment};

    const auto document = json::ValueBuilder(value).ExtractValue();
    if (transaction.Execute(sql::kSaveRfq, organization, document, expected).IsEmpty())
        throw ApiError<409>{"revision_conflict", "Запрос ставок изменён"};

    transaction.Execute(sql::kWriteRfqHistory, organization, document, identity.ActorId());
    namespace events = clients::notifications::models;
    const auto payload = [&](const std::string& recipient) {
        return events::RfqNotificationPayload{.actor_subject = identity.Subject,
            .recipient_issuer = identity.Issuer,
            .recipient_subject = recipient,
            .revision = value.revision,
            .rfq_id = value.input.id,
            .rfq_title = value.input.title};
    };
    if (!previous || previous->input.responsible_subject != value.input.responsible_subject) {
        const auto id = "rfq-assigned:" + value.input.id + ":" + std::to_string(value.revision);
        const events::PublishedEvent event{
            .value = events::RfqAssigned{.id = id,
                .organization_id = organization,
                .payload = payload(value.input.responsible_subject)}};
        platform::outbox::Enqueue(transaction, kPublishNotification, id, event);
    }
    if (value.input.status == models::RfqStatus::Ready &&
        (!previous || previous->input.status != models::RfqStatus::Ready)) {
        const auto id = "rfq-ready:" + value.input.id + ":" + std::to_string(value.revision);
        const events::PublishedEvent event{.value = events::RfqReady{.id = id,
                                               .organization_id = organization,
                                               .payload = payload(inquiry.manager_subject)}};
        platform::outbox::Enqueue(transaction, kPublishNotification, id, event);
    }

    transaction.Commit();
    return value;
}

models::Rfq RfqComponent::FindRfqRevision(
    const std::string& organization, const std::string& id, std::int64_t revision) const {
    return Read(cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetRfqRevision, organization, id, revision));
}

}  // namespace sales
