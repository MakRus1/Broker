#include <models/organization_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/organizations/members/list/view.hpp>

namespace admin_bff::views::api::admin::organizations::members::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    (void)request;
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiAdminOrganizationsMembersList;
    const auto result = deps.organizations.ApiAdminOrganizationsMembersList(
        api::Request{.body = api::RequestBody{.after = request.body.after,
                         .limit = request.body.limit,
                         .organization_id = request.body.organization_id}},
        session.Access);
    return Response200{.items = ConvertOrganizations(result.items)};
}
}  // namespace admin_bff::views::api::admin::organizations::members::list
