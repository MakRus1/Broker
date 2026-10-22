#include <models/organization_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/platform_access/update/view.hpp>

namespace admin_bff::views::api::admin::platform_access::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiAdminPlatformAccessUpdate;
    const auto result = deps.organizations.ApiAdminPlatformAccessUpdate(
        api::Request{.body = api::RequestBody{.expected_revision = request.body.expected_revision,
                         .permissions = request.body.permissions,
                         .subject = request.body.subject}},
        session.Access);
    return Response200{.access = models::PlatformAccess{.permissions = result.access.permissions,
                           .revision = result.access.revision},
        .catalog = ConvertOrganizations(result.catalog)};
}
}  // namespace admin_bff::views::api::admin::platform_access::update
