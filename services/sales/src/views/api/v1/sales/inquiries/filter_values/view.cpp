#include <helpers/registry.hpp>
#include <views/api/v1/sales/inquiries/filter_values/view.hpp>

namespace sales::views::api::v1::sales::inquiries::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    try {
        Response200 result;
        result.fields = registry::DescribeFields<models::RegistryField>(kInquiriesRegistry.Fields);
        if (!request.body.field.has_value())
            return result;
        const auto query =
            registry::BuildFilterValues(kInquiriesRegistry, request.body, Organization(context));
        const auto rows =
            deps.pg_cluster->Execute(
                               pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        for (std::size_t i = 0; i < rows.size() && i < static_cast<std::size_t>(request.body.limit);
             ++i)
            result.values.push_back({.label = rows[i].Label, .value = rows[i].Value});
        if (rows.size() > static_cast<std::size_t>(request.body.limit))
            result.next_after = result.values.back().value;
        return result;
    } catch (const registry::FilterError& error) {
        return Response400{.error = "invalid_filter", .message = error.what()};
    }
}
}  // namespace sales::views::api::v1::sales::inquiries::filter_values
