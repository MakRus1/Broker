#include <helpers/converters.hpp>
#include <helpers/locations_registry.hpp>
#include <platform/auth/identity.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/route_locations/list/view.hpp>

namespace transport_locations::views::api::admin::route_locations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transport_locations::helpers;
    if (!platform::auth::GetIdentity(context).Permissions.contains("platform.admin"))
        return Response403{
            .error = "platform_admin_required", .message = "Нужны права администратора платформы"};
    const auto scope = registry::CursorScope(request.body, "route_locations");
    if (!registry::CursorMatches(request.body, scope, EnumValue(request.body.sort) == "id"))
        return Response400{.error = "invalid_cursor",
            .message = "Курсор не соответствует фильтрам или сортировке"};
    const auto query = LocationQuery(request.body);
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
    transaction.Execute(sql::kRegistryPlan);
    auto rows =
        transaction.Execute(query.Statement, query.Parameters)
            .AsContainer<std::vector<registry::Row<pgtypes::TransportLocationV1>>>(pg::kRowTag);
    const bool more = rows.size() > static_cast<std::size_t>(request.body.limit);
    if (more)
        rows.pop_back();
    Response200 result;
    result.items = registry::ConvertItems(rows, [](const auto& row) { return Convert(row); });
    if (more) {
        result.next = boost::uuids::to_string(rows.back().Value.Id);
        result.next_cursor =
            models::LocationListCursor{.id = boost::uuids::to_string(rows.back().Value.Id),
                .scope = scope,
                .value = rows.back().SortKey};
    }
    transaction.Commit();
    return result;
}
}  // namespace transport_locations::views::api::admin::route_locations::list
