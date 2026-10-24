#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/railway_stations/save/view.hpp>

namespace transportations::views::api::admin::railway_stations::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    if (!platform::auth::GetIdentity(context).Permissions.contains("platform.admin"))
        return Response403{
            .error = "platform_admin_required", .message = "Нужны права администратора платформы"};
    const auto name = userver::utils::text::Trim(request.body.name);
    if (name.empty())
        return Response400{.error = "invalid_name", .message = "Укажите название станции"};
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kSaveRailwayStation, request.body.esr_code, name, request.body.country_code,
        request.body.existing.value_or(false));
    if (rows.IsEmpty())
        return Response409{
            .error = "station_exists", .message = "Код ЕСР уже используется в справочнике"};
    const auto row = rows.AsSingleRow<pgtypes::RailwayStationV1>();
    return Response200{.country_code = row.CountryCode, .esr_code = row.EsrCode, .name = row.Name};
}
}  // namespace transportations::views::api::admin::railway_stations::save
