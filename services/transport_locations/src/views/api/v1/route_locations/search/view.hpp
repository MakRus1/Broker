#pragma once
#include <deps.hpp>
#include <views/api/v1/route_locations/search/requests.hpp>
#include <views/api/v1/route_locations/search/responses.hpp>

namespace transport_locations::views::api::v1::route_locations::search {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace transport_locations::views::api::v1::route_locations::search
