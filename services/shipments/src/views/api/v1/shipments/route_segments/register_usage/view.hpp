#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/shipments/route_segments/register_usage/requests.hpp>
#include <views/api/v1/shipments/route_segments/register_usage/responses.hpp>

namespace shipments::views::api::v1::shipments::route_segments::register_usage {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace shipments::views::api::v1::shipments::route_segments::register_usage
