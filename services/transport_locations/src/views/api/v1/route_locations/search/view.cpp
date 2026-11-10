#include <components/locations.hpp>
#include <utility>
#include <views/api/v1/route_locations/search/view.hpp>

namespace transport_locations::views::api::v1::route_locations::search {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    return Response200{.items = deps.locations->Search(request.body.kind, request.body.query)};
}
}  // namespace transport_locations::views::api::v1::route_locations::search
