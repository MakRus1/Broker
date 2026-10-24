#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/railway_stations/list/view.hpp>

namespace transportations::views::api::admin::railway_stations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    if (!platform::auth::GetIdentity(context).Permissions.contains("platform.admin"))
        return Response403{
            .error = "platform_admin_required", .message = "Нужны права администратора платформы"};
    const auto query = userver::utils::text::Trim(request.body.query.value_or(""));
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kListRailwayStations, query, request.body.after.value_or(""));
    Response200 result;
    for (const auto& row : rows.AsSetOf<pgtypes::RailwayStationV1>()) {
        if (result.items.size() == 50) {
            result.next = result.items.back().esr_code;
            break;
        }
        result.items.push_back(Convert(row));
    }
    return result;
}
}  // namespace transportations::views::api::admin::railway_stations::list
