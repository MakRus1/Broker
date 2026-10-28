#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/railway_stations/archive/view.hpp>

namespace transport_locations::views::api::admin::railway_stations::archive {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transport_locations::helpers;
    if (!platform::auth::GetIdentity(context).Permissions.contains("platform.admin"))
        return Response403{
            .error = "platform_admin_required", .message = "Нужны права администратора платформы"};
    const auto rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kArchiveRailwayStation, request.body.id);
    if (rows.IsEmpty())
        return Response404{.error = "station_not_found", .message = "Станция не найдена"};
    return Response200{.id = request.body.id};
}
}  // namespace transport_locations::views::api::admin::railway_stations::archive
