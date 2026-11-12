#include <components/transportation_registry.hpp>
#include <converters/registry.hpp>
#include <converters/transportations.hpp>
#include <helpers/database.hpp>
#include <models/pg/route_segment.hpp>
#include <models/transportation_registry_row.hpp>
#include <registry/query.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace transportations {
namespace pg = userver::storages::postgres;
namespace registry = platform::registry;

TransportationRegistry::TransportationRegistry(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-transportations")
                   .GetCluster()) {}

userver::yaml_config::Schema TransportationRegistry::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::TransportationListResult TransportationRegistry::ListTransportations(
    models::ListTransportationsRequest request, const std::string& organization) const {
    try {
        const auto scope = platform::registry::CursorScope(request, organization);
        const auto order =
            registry::RegistryOrderBy(registry_query::registry_fields::kFields, request.sort);
        if (request.after.has_value() &&
            (request.cursor.has_value() || !order.ById || request.descending))
            throw ApiError<400>{"invalid_cursor", "Для выбранной сортировки используйте cursor"};
        if (request.cursor.has_value() && request.cursor.value().scope != scope)
            throw ApiError<400>{
                "invalid_cursor", "Фильтры или сортировка изменились. Начните с первой страницы"};

        const auto query =
            platform::registry::BuildPage(registry_query::kRegistry, request, organization);

        // Перевозки и плечи страницы должны относиться к одному снимку.
        auto transaction =
            cluster_->Begin(pg::ClusterHostType::kMaster, helpers::kReadOnlySnapshotTransaction);
        transaction.Execute(sql::kRegistryPlan);
        auto page = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<models::TransportationRegistryRow>>(pg::kRowTag);
        const bool has_more = page.size() > static_cast<std::size_t>(request.limit);
        if (has_more)
            page.pop_back();
        std::optional<models::RegistryCursor> next_cursor;
        if (has_more)
            next_cursor =
                models::RegistryCursor{.id = boost::uuids::to_string(page.back().Transportation.Id),
                    .scope = scope,
                    .value = page.back().SortKey};

        const auto rows = converters::Convert(page);
        std::vector<std::string> ids;
        ids.reserve(rows.size());
        for (const auto& row : rows)
            ids.push_back(boost::uuids::to_string(row.Id));

        // Все плечи страницы читаются одним запросом, без N+1.
        const auto route_segments = ids.empty()
            ? std::vector<models::pg::RouteSegment>{}
            : transaction.Execute(sql::kListRouteSegments, organization, ids)
                  .AsContainer<std::vector<models::pg::RouteSegment>>();
        auto result = converters::Convert(rows, route_segments);
        transaction.Commit();
        return models::TransportationListResult{
            .items = std::move(result), .next_cursor = std::move(next_cursor)};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::RegistryValuesResult TransportationRegistry::GetFilterValues(
    models::FilterValuesRequest request, const std::string& organization) const {
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
            cluster_->Begin(pg::ClusterHostType::kMaster, helpers::kReadOnlySnapshotTransaction);
        transaction.Execute(sql::kRegistryPlan);
        const auto rows = transaction.Execute(query.Statement, query.Parameters)
                              .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        transaction.Commit();

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

}  // namespace transportations
