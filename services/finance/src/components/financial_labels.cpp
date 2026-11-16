#include <components/financial_labels.hpp>
#include <converters/financial_label.hpp>
#include <converters/financial_label_input.hpp>
#include <finance/sql_queries.hpp>
#include <limits>
#include <models/errors.hpp>
#include <models/pg/financial_label_raw.hpp>
#include <platform/auth/identity.hpp>
#include <platform/registry_read.hpp>
#include <registry/financial_label_query.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/text_light.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace finance {
namespace pg = userver::storages::postgres;

FinancialLabelsComponent::FinancialLabelsComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-finance").GetCluster()) {}

userver::yaml_config::Schema FinancialLabelsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::FinancialLabelFilterValuesResult FinancialLabelsComponent::GetFinancialLabelFilterValues(
    const boost::uuids::uuid& organization,
    const models::FinancialLabelFilterValuesRequest& request) const {
    try {
        models::FinancialLabelFilterValuesResult result;
        result.fields = platform::registry::DescribeFields<models::RegistryField>(
            registry_query::labels::registry_fields::kFields);
        if (!request.field.has_value())
            return result;
        const auto query = platform::registry::BuildFilterValues(
            registry_query::labels::kRegistry, request, organization);
        const auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<platform::registry::ValueRow>>(pg::kRowTag);
        platform::registry::FillFilterValues(result, rows, request.limit);
        return result;
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::FinancialLabel FinancialLabelsComponent::SaveFinancialLabel(
    const boost::uuids::uuid& organization, const platform::auth::Identity& identity,
    const models::SaveFinancialLabelRequest& request) const {
    const auto value = converters::ToFinancialLabel(request);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto rows = request.expected_revision == 0
        ? transaction.Execute(sql::kCreateFinancialLabel, organization, value)
        : transaction.Execute(
              sql::kUpdateFinancialLabel, organization, value, request.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Метка изменена. Обновите карточку"};
    const auto result = converters::Convert(rows.AsSingleRow<models::pg::FinancialLabelRaw>());
    transaction.Execute(sql::kWriteHistory, organization, result.id, result.revision,
        identity.ActorId(), userver::formats::json::ValueBuilder(result).ExtractValue());
    transaction.Commit();
    return result;
}

models::FinancialLabel FinancialLabelsComponent::FindFinancialLabel(
    const boost::uuids::uuid& organization, const models::GetFinancialLabelRequest& request) const {
    const auto rows = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetFinancialLabel, organization, request.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Финансовая метка не найдена"};
    return converters::Convert(rows.AsSingleRow<models::pg::FinancialLabelRaw>());
}

models::FinancialLabelListResult FinancialLabelsComponent::ListFinancialLabels(
    const boost::uuids::uuid& organization,
    const models::ListFinancialLabelsRequest& request) const {
    try {
        const auto& body = request;
        const auto scope = platform::registry::CursorScope(body, organization);
        platform::registry::ValidateListCursor(body.cursor, scope);
        const auto query =
            platform::registry::BuildPage(registry_query::labels::kRegistry, body, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
            pg::TransactionOptions{
                pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
        auto rows = platform::registry::ExecuteWithCustomPlan(transaction, query)
                        .AsContainer<std::vector<models::FinancialLabelRegistryRow>>(pg::kRowTag);
        const auto cursor =
            platform::registry::FinishListPage<models::FinancialLabelRegistryCursor>(
                rows, body.limit, scope, [](const auto& row) { return row.Label.Id; });
        transaction.Commit();
        return models::FinancialLabelListResult{
            .items = converters::Convert(rows), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}
}  // namespace finance
