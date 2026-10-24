#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/admin/railway_stations/save/requests.hpp>
#include <views/api/admin/railway_stations/save/responses.hpp>

namespace transportations::views::api::admin::railway_stations::save {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace transportations::views::api::admin::railway_stations::save
