#include <models/organization_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/organizations/roles/create/view.hpp>

namespace admin_bff::views::api::admin::organizations::roles::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiAdminOrganizationsRolesCreate;
    const auto result = deps.organizations.ApiAdminOrganizationsRolesCreate(
        api::Request{.body = api::RequestBody{.description = request.body.description,
                         .id = request.body.id,
                         .name = request.body.name,
                         .organization_id = request.body.organization_id,
                         .permissions = request.body.permissions}},
        session.Access);
    return Response201{.role = Convert(result.role)};
}
}  // namespace admin_bff::views::api::admin::organizations::roles::create
