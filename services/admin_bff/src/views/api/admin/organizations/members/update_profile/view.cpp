#include <models/organization_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/organizations/members/update_profile/view.hpp>

namespace admin_bff::views::api::admin::organizations::members::update_profile {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiAdminOrganizationsMembersUpdateProfile;
    const auto result = deps.organizations.ApiAdminOrganizationsMembersUpdateProfile(
        api::Request{.body = {.first_name = request.body.first_name,
                         .last_name = request.body.last_name,
                         .middle_name = request.body.middle_name,
                         .organization_id = request.body.organization_id,
                         .subject = request.body.subject}},
        session.Access);
    return Response200{.member = Convert(result.member)};
}
}  // namespace admin_bff::views::api::admin::organizations::members::update_profile
