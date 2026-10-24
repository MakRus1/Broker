#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/railway_stations/search/requests.hpp>
#include <views/api/v1/railway_stations/search/responses.hpp>

namespace transportations::views::api::v1::railway_stations::search {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace transportations::views::api::v1::railway_stations::search
