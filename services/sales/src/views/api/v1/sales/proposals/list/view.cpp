#include <components/proposals.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/proposals/list/view.hpp>

namespace sales::views::api::v1::sales::proposals::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    return Response200{.items = deps.proposals->ListProposals(organization, request.body.id)};
}
}  // namespace sales::views::api::v1::sales::proposals::list
