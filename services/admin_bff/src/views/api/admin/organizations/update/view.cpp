#include <models/organization_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/organizations/update/view.hpp>

namespace admin_bff::views::api::admin::organizations::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {

    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiAdminOrganizationsUpdate;
    const auto result = deps.organizations.ApiAdminOrganizationsUpdate(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.organization = Convert(result.organization)};
}
}  // namespace admin_bff::views::api::admin::organizations::update
