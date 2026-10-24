#pragma once
#include <deps.hpp>
#include <views/api/geography/route_locations/requests.hpp>
#include <views/api/geography/route_locations/responses.hpp>

namespace web_bff::views::api::geography::route_locations {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::geography::route_locations
