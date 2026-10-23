#include <platform/browser/session.hpp>
#include <views/api/organizations/employees/list/view.hpp>

namespace web_bff::views::api::organizations::employees::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiOrganizationsEmployeesList;
    const auto response = deps.organizations.ApiOrganizationsEmployeesList(
        api::Request{.body = {.limit = request.body.limit,
                         .organization_id = session.User.Organization,
                         .query = request.body.query,
                         .subject = request.body.subject}},
        session.Access);
    Response200 result;
    for (const auto& item : response.items)
        result.items.push_back({.display_name = item.display_name, .subject = item.subject});
    return result;
}
}  // namespace web_bff::views::api::organizations::employees::list
