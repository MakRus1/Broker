#include <helpers/registry_query.hpp>
#include <views/api/v1/counterparties/filter_values/view.hpp>

namespace counterparties::views::api::v1::counterparties::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        Response200 result;
        result.fields = registry::DescribeFields<models::RegistryField>(registry_fields::kFields);
        if (!request.body.field.has_value())
            return result;
        const auto query =
            platform::registry::BuildFilterValues(kRegistry, request.body, organization);
        const auto rows =
            deps.pg_cluster->Execute(
                               pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
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
}  // namespace counterparties::views::api::v1::counterparties::filter_values
