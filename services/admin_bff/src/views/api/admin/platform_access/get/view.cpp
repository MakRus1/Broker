#include <models/organization_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/platform_access/get/view.hpp>

namespace admin_bff::views::api::admin::platform_access::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiAdminPlatformAccessGet;
    const auto result = deps.organizations.ApiAdminPlatformAccessGet(
        api::Request{.body = api::RequestBody{.subject = request.body.subject}}, session.Access);
    return Response200{.access = models::PlatformAccess{.permissions = result.access.permissions,
                           .revision = result.access.revision},
        .catalog = ConvertOrganizations(result.catalog)};
}
}  // namespace admin_bff::views::api::admin::platform_access::get
