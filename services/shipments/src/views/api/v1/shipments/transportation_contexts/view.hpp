#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/shipments/transportation_contexts/requests.hpp>
#include <views/api/v1/shipments/transportation_contexts/responses.hpp>

namespace shipments::views::api::v1::shipments::transportation_contexts {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace shipments::views::api::v1::shipments::transportation_contexts
