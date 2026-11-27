#include <line_operations/voyages.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/voyages/list/view.hpp>

namespace shipments::views::api::v1::voyages::list {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto org = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    auto voyage = line_operations::ListVoyages(*deps.pg_cluster, org);
    return Response200{.items = std::move(voyage.items)};
}
}  // namespace shipments::views::api::v1::voyages::list
