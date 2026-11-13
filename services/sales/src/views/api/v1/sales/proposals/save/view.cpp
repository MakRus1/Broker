#include <components/inquiries.hpp>
#include <components/proposal_calculation.hpp>
#include <components/proposals.hpp>
#include <converters/inputs.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/sales/proposals/save/view.hpp>

namespace sales::views::api::v1::sales::proposals::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);
    const auto& token = platform::auth::GetAccessToken(context);
    const auto inquiry = deps.inquiries->FindInquiry(organization, request.body.inquiry_id);
    auto input = converters::ToProposalInput(request.body);
    proposal_rules::ValidateInput(input, inquiry);

    const auto data =
        deps.proposals->PrepareProposalCalculation(organization, token, inquiry, input);
    auto value = proposal_rules::CalculateProposal(std::move(input), data);

    return Response200{.item = deps.proposals->SaveProposal(
                           organization, actor, request.body.expected_revision, std::move(value))};
}
}  // namespace sales::views::api::v1::sales::proposals::save
