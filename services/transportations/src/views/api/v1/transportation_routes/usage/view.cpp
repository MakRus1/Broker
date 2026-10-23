#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/inherited_route.hpp>
#include <platform/auth/identity.hpp>
#include <set>
#include <views/api/v1/transportation_routes/usage/view.hpp>

namespace transportations::views::api::v1::transportation_routes::usage {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        const auto rows = deps.pg_cluster
                              ->Execute(pg::ClusterHostType::kMaster, sql::kRouteUsage,
                                  organization, request.body.shipment_id)
                              .AsContainer<std::vector<Json>>();
        std::set<std::string> ids;
        for (const auto& segment : Convert(rows))
            if (HasRouteData(segment))
                ids.insert(segment.id);
        return Response200{.segment_ids = {ids.begin(), ids.end()}};
    } catch (const ::transportations::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        case 503:
            return Response503{.error = error.Code, .message = error.what()};
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace transportations::views::api::v1::transportation_routes::usage
