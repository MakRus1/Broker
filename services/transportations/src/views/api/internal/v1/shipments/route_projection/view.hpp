#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/internal/v1/shipments/route_projection/requests.hpp>
#include <views/api/internal/v1/shipments/route_projection/responses.hpp>

namespace transportations::views::api::internal::v1::shipments::route_projection {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace transportations::views::api::internal::v1::shipments::route_projection
