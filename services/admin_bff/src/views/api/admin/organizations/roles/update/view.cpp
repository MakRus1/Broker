#include <models/organization_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/organizations/roles/update/view.hpp>

namespace admin_bff::views::api::admin::organizations::roles::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiAdminOrganizationsRolesUpdate;
    const auto result = deps.organizations.ApiAdminOrganizationsRolesUpdate(
        api::Request{.body = api::RequestBody{.description = request.body.description,
                         .expected_revision = request.body.expected_revision,
                         .id = request.body.id,
                         .name = request.body.name,
                         .organization_id = request.body.organization_id,
                         .permissions = request.body.permissions}},
        session.Access);
    return Response200{.role = Convert(result.role)};
}
}  // namespace admin_bff::views::api::admin::organizations::roles::update
