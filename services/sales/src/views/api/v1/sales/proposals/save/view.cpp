#include <components/inquiries.hpp>
#include <components/proposal_calculator.hpp>
#include <components/proposals.hpp>
#include <converters/inputs.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/proposals/save/view.hpp>

namespace sales::views::api::v1::sales::proposals::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);
    const auto inquiry = deps.inquiries->FindInquiry(organization, request.body.inquiry_id);
    auto value =
        deps.calculator->CalculateProposal(organization, platform::auth::GetAccessToken(context),
            inquiry, converters::ToProposalInput(request.body));

    return Response200{.item = deps.proposals->SaveProposal(
                           organization, actor, request.body.expected_revision, std::move(value))};
}
}  // namespace sales::views::api::v1::sales::proposals::save
