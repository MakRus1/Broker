#include <models/organization_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/organizations/create/view.hpp>

namespace admin_bff::views::api::admin::organizations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    (void)request;
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiAdminOrganizationsCreate;
    const auto result = deps.organizations.ApiAdminOrganizationsCreate(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response201{.organization = Convert(result.organization)};
}
}  // namespace admin_bff::views::api::admin::organizations::create
