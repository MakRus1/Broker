#include <helpers/registry_query.hpp>
#include <views/api/v1/transportations/filter_values/view.hpp>

namespace transportations::views::api::v1::transportations::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        const bool route = request.body.scope == models::FilterValuesScope::Route;
        Response200 result;
        result.fields = registry::DescribeFields<models::RegistryField>(route
                ? std::span<const registry::RegistryField>{route_registry::kFields}
                : std::span<const registry::RegistryField>{registry_fields::kFields});
        if (!request.body.field.has_value())
            return result;
        if (route && request.body.candidates.has_value())
            throw registry::FilterError{"Для участка используйте поиск значений, а не candidates"};
        if (route &&
            request.body.group > static_cast<int>(request.body.registry.route_filters.has_value()
                                         ? request.body.registry.route_filters.value().size()
                                         : 0))
            throw registry::FilterError{"Некорректный номер группы участка"};
        if (!route && request.body.group != 0)
            throw registry::FilterError{"Номер группы допустим только для участка"};
        const auto query = route
            ? route_registry::BuildValues(request.body.registry, organization,
                  request.body.field.value(), request.body.group, request.body.query,
                  request.body.after, request.body.limit,
                  [&](auto& builder) {
                      registry::ApplyFilters(
                          builder, registry_fields::kFields, request.body.registry.filters);
                  })
            : platform::registry::BuildFilterValues(kRegistry, request.body, organization);
        // Custom plan сохраняет селективность условий и вариантов связанной коллекции.
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        transaction.Execute(sql::kRegistryPlan);
        const auto rows = transaction.Execute(query.Statement, query.Parameters)
                              .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        transaction.Commit();
        const bool more = rows.size() > static_cast<std::size_t>(request.body.limit);
        for (std::size_t index = 0;
             index < rows.size() && index < static_cast<std::size_t>(request.body.limit); ++index)
            result.values.push_back({.label = rows[index].Label, .value = rows[index].Value});
        if (more)
            result.next_after = result.values.back().value;
        return result;
    } catch (const registry::FilterError& error) {
        return Response400{.error = "invalid_filter", .message = error.what()};
    }
}
}  // namespace transportations::views::api::v1::transportations::filter_values
