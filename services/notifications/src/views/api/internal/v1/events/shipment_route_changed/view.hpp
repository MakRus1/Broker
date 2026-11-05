#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/internal/v1/events/shipment_route_changed/requests.hpp>
#include <views/api/internal/v1/events/shipment_route_changed/responses.hpp>

namespace notifications::views::api::internal::v1::events::shipment_route_changed {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace notifications::views::api::internal::v1::events::shipment_route_changed
