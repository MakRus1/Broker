#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/geography/railway_stations/requests.hpp>
#include <views/api/geography/railway_stations/responses.hpp>

namespace web_bff::views::api::geography::railway_stations {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::geography::railway_stations
