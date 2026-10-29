#include <helpers/registry_query.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/transportations/route_filter_values/view.hpp>

namespace transportations::views::api::v1::transportations::route_filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        Response200 result;
        result.fields = registry::DescribeFields<models::RegistryField>(route_registry::kFields);
        if (!request.body.field.has_value())
            return result;
        if (request.body.registry.route_filters.has_value() &&
            request.body.group >
                static_cast<int>(request.body.registry.route_filters.value().size()))
            throw registry::FilterError{"Некорректный номер группы участка"};
        const auto query = route_registry::BuildValues(request.body.registry, organization,
            request.body.field.value(), request.body.group, request.body.query, request.body.after,
            request.body.limit, [&](auto& builder) {
                registry::ApplyFilters(
                    builder, registry_fields::kFields, request.body.registry.filters);
            });
        // Generic plan теряет селективность станции; SET LOCAL ограничен этой транзакцией.
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        transaction.Execute(sql::kRegistryPlan);
        auto rows = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        transaction.Commit();
        const bool more = rows.size() > static_cast<std::size_t>(request.body.limit);
        if (more)
            rows.pop_back();
        for (const auto& row : rows)
            result.values.push_back({.label = row.Label, .value = row.Value});
        if (more)
            result.next_after = result.values.back().value;
        return result;
    } catch (const registry::FilterError& error) {
        return Response400{.error = "invalid_filter", .message = error.what()};
    }
}
}  // namespace transportations::views::api::v1::transportations::route_filter_values
