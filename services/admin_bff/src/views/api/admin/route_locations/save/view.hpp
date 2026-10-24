#pragma once
#include <deps.hpp>
#include <views/api/admin/route_locations/save/requests.hpp>
#include <views/api/admin/route_locations/save/responses.hpp>

namespace admin_bff::views::api::admin::route_locations::save {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace admin_bff::views::api::admin::route_locations::save
