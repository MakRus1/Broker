#include <components/contract_registry.hpp>
#include <contracts/sql_queries.hpp>
#include <converters/contracts.hpp>
#include <models/contract_registry_row.hpp>
#include <models/errors.hpp>
#include <registry/contract_query.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace contracts {
namespace pg = userver::storages::postgres;

ContractRegistry::ContractRegistry(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-contracts").GetCluster()) {
}

userver::yaml_config::Schema ContractRegistry::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::ContractListResult ContractRegistry::ListContracts(
    const std::string& organization, const models::ListContractsRequest& request) const {
    try {
        const auto& body = request;
        const auto scope = platform::registry::CursorScope(body, organization);
        const auto order = registry::RegistryOrderBy(registry_fields::kFields, body.sort);
        const bool date_sort = order.CursorType == registry::CursorKeyType::Date;
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }

        const auto query =
            platform::registry::BuildPage(registry_query::kRegistry, body, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
            pg::TransactionOptions{
                pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
        transaction.Execute(sql::kRegistryPlan);
        auto rows = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<models::ContractRegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        std::optional<models::ContractRegistryCursor> cursor;
        if (more) {
            const auto& last = rows.back();
            cursor = models::ContractRegistryCursor{
                .date_value = date_sort ? last.DateKey : std::nullopt,
                .id = boost::uuids::to_string(last.Contract.Id),
                .scope = scope,
                .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        transaction.Commit();

        std::vector<models::Contract> items;
        items.reserve(rows.size());
        for (const auto& row : rows)
            items.push_back(converters::Convert(row.Contract));
        return models::ContractListResult{.items = std::move(items), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::ContractFilterValuesResult ContractRegistry::GetContractFilterValues(
    const std::string& organization, const models::ContractFilterValuesRequest& request) const {
    try {
        models::ContractFilterValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(registry_fields::kFields);
        if (!request.field.has_value())
            return result;
        const auto query =
            platform::registry::BuildFilterValues(registry_query::kRegistry, request, organization);
        const auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(request.limit);
        for (std::size_t index = 0;
             index < rows.size() && index < static_cast<std::size_t>(request.limit); ++index)
            result.values.push_back({.label = rows[index].Label, .value = rows[index].Value});
        if (more)
            result.next_after = result.values.back().value;
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}
}  // namespace contracts
