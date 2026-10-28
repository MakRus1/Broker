#include <helpers/converters.hpp>
#include <helpers/stations_registry.hpp>
#include <platform/auth/identity.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/railway_stations/list/view.hpp>

namespace transport_locations::views::api::admin::railway_stations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transport_locations::helpers;
    if (!platform::auth::GetIdentity(context).Permissions.contains("platform.admin"))
        return Response403{
            .error = "platform_admin_required", .message = "Нужны права администратора платформы"};
    const auto scope = registry::CursorScope(request.body, "railway_stations");
    if (!registry::CursorMatches(request.body, scope, EnumValue(request.body.sort) == "esr_code"))
        return Response400{.error = "invalid_cursor",
            .message = "Курсор не соответствует фильтрам или сортировке"};
    const auto query = StationQuery(request.body);
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
    transaction.Execute(sql::kRegistryPlan);
    auto rows =
        transaction.Execute(query.Statement, query.Parameters)
            .AsContainer<std::vector<registry::Row<pgtypes::RailwayStationV1>>>(pg::kRowTag);
    const bool more = rows.size() > static_cast<std::size_t>(request.body.limit);
    if (more)
        rows.pop_back();
    Response200 result;
    result.items = registry::ConvertItems(rows, [](const auto& row) { return Convert(row); });
    if (more) {
        result.next = rows.back().Value.EsrCode;
        result.next_cursor = models::StationListCursor{
            .id = rows.back().Value.EsrCode, .scope = scope, .value = rows.back().SortKey};
    }
    transaction.Commit();
    return result;
}
}  // namespace transport_locations::views::api::admin::railway_stations::list
