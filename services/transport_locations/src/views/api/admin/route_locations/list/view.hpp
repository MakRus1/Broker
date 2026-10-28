#pragma once
#include <deps.hpp>
#include <views/api/admin/route_locations/list/requests.hpp>
#include <views/api/admin/route_locations/list/responses.hpp>

namespace transport_locations::views::api::admin::route_locations::list {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace transport_locations::views::api::admin::route_locations::list
