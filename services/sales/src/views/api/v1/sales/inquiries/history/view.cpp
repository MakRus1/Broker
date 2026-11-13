#include <components/inquiries.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/inquiries/history/view.hpp>

namespace sales::views::api::v1::sales::inquiries::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    return Response200{.items = deps.inquiries->GetInquiryHistory(organization, request.body)};
}
}  // namespace sales::views::api::v1::sales::inquiries::history
