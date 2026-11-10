#include <components/railway_stations.hpp>
#include <utility>
#include <views/api/v1/railway_stations/search/view.hpp>

namespace transport_locations::views::api::v1::railway_stations::search {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    return Response200{.items = deps.stations->Search(request.body.query)};
}
}  // namespace transport_locations::views::api::v1::railway_stations::search
