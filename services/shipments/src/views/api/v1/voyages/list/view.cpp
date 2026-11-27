#include <components/voyages.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/voyages/list/view.hpp>

namespace shipments::views::api::v1::voyages::list {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto org = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    auto voyage = deps.voyages->ListVoyages(org);
    return Response200{.items = std::move(voyage.items)};
}
}  // namespace shipments::views::api::v1::voyages::list
