#include <models/organization_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/organizations/members/update/view.hpp>

namespace admin_bff::views::api::admin::organizations::members::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    (void)request;
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiAdminOrganizationsMembersUpdate;
    const auto result = deps.organizations.ApiAdminOrganizationsMembersUpdate(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.member = Convert(result.member)};
}
}  // namespace admin_bff::views::api::admin::organizations::members::update
