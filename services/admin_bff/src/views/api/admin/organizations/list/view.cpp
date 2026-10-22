#include <models/organization_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/organizations/list/view.hpp>

namespace admin_bff::views::api::admin::organizations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {

    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiAdminOrganizationsList;
    const auto result = deps.organizations.ApiAdminOrganizationsList(
        api::Request{
            .body = api::RequestBody{.after = request.body.after, .limit = request.body.limit}},
        session.Access);
    return Response200{.items = ConvertOrganizations(result.items)};
}
}  // namespace admin_bff::views::api::admin::organizations::list
