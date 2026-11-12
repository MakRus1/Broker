#include <components/proposals.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/proposals/action/view.hpp>

namespace sales::views::api::v1::sales::proposals::action {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    return Response200{
        .item = deps.proposals->ApplyProposalAction(organization, actor, request.body)};
}
}  // namespace sales::views::api::v1::sales::proposals::action
