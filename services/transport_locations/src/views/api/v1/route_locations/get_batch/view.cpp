#include <boost/uuid/string_generator.hpp>
#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/route_locations/get_batch/view.hpp>

namespace transport_locations::views::api::v1::route_locations::get_batch {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    using namespace ::transport_locations::helpers;
    std::vector<boost::uuids::uuid> ids;
    ids.reserve(request.body.ids.size());
    for (const auto& id : request.body.ids)
        ids.push_back(boost::uuids::string_generator{}(id));
    const auto rows =
        deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kGetRouteLocations, ids);
    Response200 result;
    for (const auto& row : rows.AsSetOf<pgtypes::TransportLocationV1>())
        result.items.push_back(Convert(row));
    return result;
}
}  // namespace transport_locations::views::api::v1::route_locations::get_batch
