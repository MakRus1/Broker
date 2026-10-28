#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/v1/route_locations/search/view.hpp>

namespace transport_locations::views::api::v1::route_locations::search {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    using namespace ::transport_locations::helpers;
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kSearchRouteLocations, models::EnumValue(request.body.kind),
        userver::utils::text::Trim(request.body.query.value_or("")));
    Response200 result;
    for (const auto& row : rows.AsSetOf<pgtypes::TransportLocationV1>()) {
        result.items.push_back(Convert(row));
    }
    return result;
}
}  // namespace transport_locations::views::api::v1::route_locations::search
