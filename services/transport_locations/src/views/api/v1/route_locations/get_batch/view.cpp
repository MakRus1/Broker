#include <components/locations.hpp>
#include <utility>
#include <views/api/v1/route_locations/get_batch/view.hpp>

namespace transport_locations::views::api::v1::route_locations::get_batch {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    return Response200{.items = deps.locations->GetBatch(request.body.ids)};
}
}  // namespace transport_locations::views::api::v1::route_locations::get_batch
