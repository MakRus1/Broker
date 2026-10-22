#include <models/organization_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/organizations/members/create/view.hpp>

namespace admin_bff::views::api::admin::organizations::members::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {

    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiAdminOrganizationsMembersCreate;
    const auto result = deps.organizations.ApiAdminOrganizationsMembersCreate(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response201{.member = Convert(result.member)};
}
}  // namespace admin_bff::views::api::admin::organizations::members::create
