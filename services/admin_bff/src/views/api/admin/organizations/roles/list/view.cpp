#include <models/organization_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/organizations/roles/list/view.hpp>

namespace admin_bff::views::api::admin::organizations::roles::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiAdminOrganizationsRolesList;
    const auto result = deps.organizations.ApiAdminOrganizationsRolesList(
        api::Request{.body = api::RequestBody{.organization_id = request.body.organization_id}},
        session.Access);
    return Response200{.items = ConvertOrganizations(result.items),
        .permissions = ConvertOrganizations(result.permissions)};
}
}  // namespace admin_bff::views::api::admin::organizations::roles::list
