#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/shipment_items/add/requests.hpp>
#include <views/api/v1/shipment_items/add/responses.hpp>

namespace shipments::views::api::v1::shipment_items::add {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace shipments::views::api::v1::shipment_items::add
