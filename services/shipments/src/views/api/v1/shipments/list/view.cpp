#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/shipments/list/view.hpp>

namespace shipments::views::api::v1::shipments::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        const auto tenant = ResolveTenant(*deps.pg_cluster, principal);
        const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
            sql::kListShipments, tenant, request.body.after, request.body.limit);
        return Response200{.items = Convert(rows)};
    } catch (const ::shipments::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace shipments::views::api::v1::shipments::list
