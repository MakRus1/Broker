#include <components/cards.hpp>
#include <helpers/settings.hpp>
#include <models/errors.hpp>
#include <sales/sql_queries.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace models = sales::models;
using sales::ApiError;

void ValidateStage(const models::SalesSettings& settings, const std::string& id,
    const std::optional<std::string>& previous) {
    for (const auto& stage : settings.stages)
        if (stage.id == id && (stage.enabled || previous == id))
            return;
    throw ApiError<400>{"invalid_stage", "Выберите действующий этап продаж"};
}
}  // namespace

namespace sales {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

CardsComponent::CardsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()) {}

userver::yaml_config::Schema CardsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::SalesCard CardsComponent::SaveCard(const std::string& organization,
    const platform::auth::Identity& identity, const models::SaveSalesCardRequest& request) const {
    const auto& value = request;

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockSettings, organization);
    const auto previous = transaction.Execute(sql::kLockCards, organization, value.id);
    const auto previous_stage =
        previous.IsEmpty() ? std::nullopt : std::optional{previous.AsSingleRow<std::string>()};
    ValidateStage(helpers::ReadSalesSettings(transaction.Execute(sql::kGetSettings, organization)),
        value.stage_id, previous_stage);

    const auto document = json::ValueBuilder(value).ExtractValue();
    transaction.Execute(sql::kClearChangedActionSource, organization, document);
    const auto changed = value.expected_revision == 0
        ? transaction.Execute(sql::kCreateCards, organization, document)
        : transaction.Execute(sql::kUpdateCards, organization, document, value.expected_revision);
    if (changed.IsEmpty())
        throw ApiError<409>{
            "revision_conflict", "Запись уже существует, изменена или передана в поставку"};
    const auto rows = transaction.Execute(sql::kGetCards, organization, value.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    const auto result = models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::SalesCard>{});

    transaction.Execute(sql::kWriteHistory, organization, std::string{"cards"}, result.id,
        result.revision, identity.ActorId(), json::ValueBuilder(result).ExtractValue());

    transaction.Commit();

    return result;
}

models::SalesCard CardsComponent::FindCard(
    const std::string& organization, const std::string& id) const {
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetCards, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    return models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::SalesCard>{});
}

std::vector<models::SalesHistoryEntry> CardsComponent::GetCardHistory(
    const std::string& organization, const models::HistoryRequest& request) const {
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kHistoryCards,
        organization, request.id, request.after_revision.value_or(0));
    std::vector<models::SalesHistoryEntry> response;
    response.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<json::Value>())
        response.push_back(
            models::ParseModelJson(row, std::type_identity<models::SalesHistoryEntry>{}));

    return response;
}

}  // namespace sales
