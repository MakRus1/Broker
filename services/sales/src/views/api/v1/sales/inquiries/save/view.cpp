#include <components/inquiries.hpp>
#include <components/sales_directory.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/inquiries/save/view.hpp>

namespace sales::views::api::v1::sales::inquiries::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    deps.directory->ValidateSalesParties(organization, platform::auth::GetAccessToken(context),
        request.body.customer_id, request.body.manager_subject);

    return Response200{.item = deps.inquiries->SaveInquiry(organization, actor, request.body)};
}
}  // namespace sales::views::api::v1::sales::inquiries::save
