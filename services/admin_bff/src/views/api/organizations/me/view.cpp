#include <models/organization_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/organizations/me/view.hpp>

namespace admin_bff::views::api::organizations::me {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {

    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiOrganizationsMe;
    const auto result = deps.organizations.ApiOrganizationsMe({}, session.Access);
    return Response200{.managed_organization_ids = result.managed_organization_ids,
        .organizations = ConvertOrganizations(result.organizations),
        .platform_admin = result.platform_admin};
}
}  // namespace admin_bff::views::api::organizations::me
