#include <helpers/registry.hpp>
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
            .body = api::RequestBody{.after = request.body.after,
                .cursor =
                    ConvertRegistryCursor<clients::organizations::models::OrganizationListCursor>(
                        request.body.cursor),
                .descending = request.body.descending,
                .enabled = request.body.enabled,
                .limit = request.body.limit,
                .query = request.body.query,
                .sort = clients::organizations::models::ParseEnumScalar(
                    EnumValue(request.body.sort), decltype(api::RequestBody{}.sort){})}},
        session.Access);
    return Response200{.items = ConvertOrganizations(result.items),
        .next_cursor = ConvertRegistryCursor<models::OrganizationListCursor>(result.next_cursor)};
}
}  // namespace admin_bff::views::api::admin::organizations::list
