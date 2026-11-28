#include <components/transportation_registry.hpp>
#include <converters/registry.hpp>
#include <converters/transportations.hpp>
#include <helpers/database.hpp>
#include <models/transportation_registry_row.hpp>
#include <platform/db.hpp>
#include <platform/registry_read.hpp>
#include <registry/query.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>

namespace transportations {
namespace pg = userver::storages::postgres;
namespace registry = platform::registry;

TransportationRegistry::TransportationRegistry(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-transportations")
                   .GetCluster()) {}

models::TransportationListResult TransportationRegistry::ListTransportations(
    const boost::uuids::uuid& organization,
    const models::ListTransportationsRequest& request) const {
    // Все данные карточек страницы читаются одним запросом.
    const auto page = platform::registry::ReadPage<models::TransportationRegistryRow>(cluster_,
        registry_query::kRegistry, request, organization,
        [](const auto& row) { return row.Transportation.Id; });
    return models::TransportationListResult{
        .items = converters::Convert(converters::Convert(page.Rows)),
        .next_cursor = page.NextCursor};
}

models::RegistryValuesResult TransportationRegistry::GetFilterValues(
    const boost::uuids::uuid& organization, const models::FilterValuesRequest& request) const {
    try {
        const bool route = request.scope == models::FilterValuesScope::Route;
        models::RegistryValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(route
                ? std::span<const registry::RegistryField>{registry_query::route_registry::kFields}
                : std::span<const registry::RegistryField>{
                      registry_query::registry_fields::kFields});
        if (!request.field.has_value())
            return result;
        if (route && request.candidates.has_value())
            throw registry::FilterError{"Для участка используйте поиск значений, а не candidates"};
        if (route &&
            request.group > static_cast<int>(request.registry.route_filters.has_value()
                                    ? request.registry.route_filters.value().size()
                                    : 0))
            throw registry::FilterError{"Некорректный номер группы участка"};
        if (!route && request.group != 0)
            throw registry::FilterError{"Номер группы допустим только для участка"};

        const auto query = route
            ? registry_query::route_registry::BuildValues(request.registry, organization,
                  request.field.value(), request.group, request.query, request.after, request.limit,
                  [&](auto& builder) {
                      registry::ApplyFilters(builder, registry_query::registry_fields::kFields,
                          request.registry.filters);
                  })
            : platform::registry::BuildFilterValues(
                  registry_query::kRegistry, request, organization);
        // Custom plan сохраняет селективность условий и вариантов связанной коллекции.
        auto transaction =
            cluster_->Begin(pg::ClusterHostType::kMaster, platform::db::kReadOnlySnapshot);
        const auto rows = platform::registry::ExecuteWithCustomPlan(transaction, query)
                              .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        transaction.Commit();

        platform::registry::FillFilterValues(result, rows, request.limit);
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

}  // namespace transportations
