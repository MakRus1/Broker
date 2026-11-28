#include <components/contract_registry.hpp>
#include <contracts/sql_queries.hpp>
#include <converters/contracts.hpp>
#include <models/contract_registry_row.hpp>
#include <models/errors.hpp>
#include <platform/registry_read.hpp>
#include <registry/contract_query.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>

namespace contracts {
namespace pg = userver::storages::postgres;

ContractRegistry::ContractRegistry(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-contracts").GetCluster()) {
}

models::ContractListResult ContractRegistry::ListContracts(
    const boost::uuids::uuid& organization, const models::ListContractsRequest& request) const {
    const auto page = platform::registry::ReadPage<models::ContractRegistryRow>(cluster_,
        registry_query::kRegistry, request, organization,
        [](const auto& row) { return row.Contract.Id; });
    std::vector<models::Contract> items;
    items.reserve(page.Rows.size());
    for (const auto& row : page.Rows)
        items.push_back(converters::Convert(row.Contract));
    return models::ContractListResult{.items = std::move(items), .next_cursor = page.NextCursor};
}

models::ContractFilterValuesResult ContractRegistry::GetContractFilterValues(
    const boost::uuids::uuid& organization,
    const models::ContractFilterValuesRequest& request) const {
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
        platform::registry::FillFilterValues(result, rows, request.limit);
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}
}  // namespace contracts
