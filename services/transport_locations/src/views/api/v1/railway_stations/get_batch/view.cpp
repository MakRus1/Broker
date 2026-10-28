#include <boost/uuid/string_generator.hpp>
#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/railway_stations/get_batch/view.hpp>

namespace transport_locations::views::api::v1::railway_stations::get_batch {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    using namespace ::transport_locations::helpers;
    const auto rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetRailwayStations, request.body.ids);
    Response200 result;
    for (const auto& row : rows.AsSetOf<pgtypes::RailwayStationV2>())
        result.items.push_back(Convert(row));
    return result;
}
}  // namespace transport_locations::views::api::v1::railway_stations::get_batch
