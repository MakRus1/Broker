#include <components/counterparty_registry.hpp>
#include <converters/counterparties.hpp>
#include <models/counterparty_registry_row.hpp>
#include <models/errors.hpp>
#include <platform/registry_read.hpp>
#include <registry/query.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>

namespace counterparties {
namespace pg = userver::storages::postgres;
using converters::Convert;
using registry_query::kRegistry;

CounterpartyRegistry::CounterpartyRegistry(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-counterparties")
                   .GetCluster()) {}

models::CounterpartyListResult CounterpartyRegistry::ListCounterparties(
    const boost::uuids::uuid& organization,
    const models::ListCounterpartiesRequest& request) const {
    const auto page = platform::registry::ReadPage<models::CounterpartyRegistryRow>(cluster_,
        kRegistry, request, organization, [](const auto& row) { return row.Counterparty.Id; });
    std::vector<models::Counterparty> items;
    items.reserve(page.Rows.size());
    for (const auto& row : page.Rows)
        items.push_back(Convert(row.Counterparty));
    return models::CounterpartyListResult{
        .items = std::move(items), .next_cursor = page.NextCursor};
}

models::CounterpartyFilterValuesResult CounterpartyRegistry::GetCounterpartyFilterValues(
    const boost::uuids::uuid& organization,
    const models::CounterpartyFilterValuesRequest& request) const {
    try {
        models::CounterpartyFilterValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(registry_fields::kFields);
        if (!request.field.has_value())
            return result;
        const auto query = platform::registry::BuildFilterValues(kRegistry, request, organization);
        const auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        platform::registry::FillFilterValues(result, rows, request.limit);
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

}  // namespace counterparties
