#include <components/inquiries.hpp>
#include <components/rfq.hpp>
#include <components/rfq_rules.hpp>
#include <converters/inputs.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/sales/rfqs/save/view.hpp>

namespace sales::views::api::v1::sales::rfqs::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);
    const auto& token = platform::auth::GetAccessToken(context);
    const auto inquiry = deps.inquiries->FindInquiry(organization, request.body.inquiry_id);
    auto input = converters::ToRfqInput(request.body);
    rfq_rules::ValidateInput(input);

    deps.rfq->PrepareRfq(organization, token, inquiry, input);

    return Response200{.item = deps.rfq->SaveRfq(
                           organization, actor, request.body.expected_revision, std::move(input))};
}
}  // namespace sales::views::api::v1::sales::rfqs::save
