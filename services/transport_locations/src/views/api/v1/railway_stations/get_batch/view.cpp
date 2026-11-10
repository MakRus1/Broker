#include <components/railway_stations.hpp>
#include <utility>
#include <views/api/v1/railway_stations/get_batch/view.hpp>

namespace transport_locations::views::api::v1::railway_stations::get_batch {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    return Response200{.items = deps.stations->GetBatch(request.body.ids)};
}
}  // namespace transport_locations::views::api::v1::railway_stations::get_batch
