#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <userver/utils/text.hpp>
#include <views/api/v1/railway_stations/search/view.hpp>

namespace transportations::views::api::v1::railway_stations::search {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const auto query = userver::utils::text::Trim(request.body.query);
    if (query.size() < 2)
        return Response400{.error = "invalid_query", .message = "Введите название или код станции"};
    const auto rows = deps.pg_cluster->Execute(
        helpers::pg::ClusterHostType::kSlave, sql::kSearchRailwayStations, query);
    Response200 result;
    for (const auto& row : rows.AsSetOf<pgtypes::RailwayStationV1>())
        result.items.push_back(helpers::Convert(row));
    return result;
}
}  // namespace transportations::views::api::v1::railway_stations::search
