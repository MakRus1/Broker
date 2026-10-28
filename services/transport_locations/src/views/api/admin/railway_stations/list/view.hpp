#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/admin/railway_stations/list/requests.hpp>
#include <views/api/admin/railway_stations/list/responses.hpp>

namespace transport_locations::views::api::admin::railway_stations::list {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace transport_locations::views::api::admin::railway_stations::list
