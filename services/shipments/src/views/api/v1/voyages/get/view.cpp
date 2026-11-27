#include <line_operations/voyages.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/voyages/get/view.hpp>

namespace shipments::views::api::v1::voyages::get {
Response View::Handle(Request&& request, userver::server::request::RequestContext& context,
                      Deps deps) {
    const auto org = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    auto voyage = line_operations::GetVoyage(*deps.pg_cluster, org, request.body.voyage_id);
    return Response200{.bookings = std::move(voyage.bookings),
                       .id = std::move(voyage.id),
                       .number = std::move(voyage.number),
                       .ports = std::move(voyage.ports)};
}
}  // namespace shipments::views::api::v1::voyages::get
