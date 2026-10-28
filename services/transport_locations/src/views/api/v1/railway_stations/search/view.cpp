#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <userver/utils/text.hpp>
#include <views/api/v1/railway_stations/search/view.hpp>

namespace transport_locations::views::api::v1::railway_stations::search {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const auto query = userver::utils::text::Trim(request.body.query);
    const auto rows = deps.pg_cluster->Execute(
        helpers::pg::ClusterHostType::kSlave, sql::kSearchRailwayStations, query);
    Response200 result;
    for (const auto& row : rows.AsSetOf<pgtypes::RailwayStationV2>())
        result.items.push_back(helpers::Convert(row));
    return result;
}
}  // namespace transport_locations::views::api::v1::railway_stations::search
