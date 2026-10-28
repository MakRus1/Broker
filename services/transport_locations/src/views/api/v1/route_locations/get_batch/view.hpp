#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/route_locations/get_batch/requests.hpp>
#include <views/api/v1/route_locations/get_batch/responses.hpp>

namespace transport_locations::views::api::v1::route_locations::get_batch {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace transport_locations::views::api::v1::route_locations::get_batch
