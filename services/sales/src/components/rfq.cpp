#include <algorithm>
#include <components/rfq.hpp>
#include <events/shipment_command.hpp>
#include <helpers/editable_inquiry.hpp>
#include <models/errors.hpp>
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
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()) {}

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

void RfqComponent::ResolveQuote(const std::string& organization,
    const models::ProposalInput& proposal, const models::Inquiry& inquiry,
    models::ProposalLine& line) const {
    if (!line.quote_source)
        return;
    const auto& source = *line.quote_source;
    const auto value = Read(cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetRfqRevision,
        organization, source.rfq_id, source.revision));
    if (value.input.inquiry_id != proposal.inquiry_id ||
        value.input.status != models::RfqStatus::Ready)
        throw ApiError<400>{"wrong_quote_source", "Выберите готовую котировку этого запроса"};
    const auto segment = std::find_if(inquiry.route_segments.begin(), inquiry.route_segments.end(),
        [&](const auto& candidate) { return candidate.id == value.input.route_segment_id; });
    if (segment == inquiry.route_segments.end() ||
        json::ValueBuilder(*segment).ExtractValue() !=
            json::ValueBuilder(value.route_segment).ExtractValue())
        throw ApiError<400>{
            "quote_route_changed", "Участок изменился. Уточните котировку у поставщика"};

    for (const auto& quote : value.input.quotes) {
        if (quote.id != source.quote_id)
            continue;
        if (quote.valid_until && *quote.valid_until < proposal.calculation_date)
            throw ApiError<400>{"quote_expired", "Срок действия котировки истёк"};
        for (const auto& quoted : quote.lines) {
            if (quoted.id != source.line_id)
                continue;
            line.label_id = quoted.label_id;
            line.unit = quoted.unit;
            line.quantity_milli = quoted.quantity_milli;
            line.purchase = quoted.price;
            line.supplier_id = quote.supplier_id;
            line.route_segment_id = value.input.route_segment_id;
            return;
        }
    }
    throw ApiError<400>{"quote_not_found", "Строка котировки не найдена"};
}

}  // namespace sales
