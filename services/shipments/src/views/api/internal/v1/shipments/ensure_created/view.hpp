#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/internal/v1/shipments/ensure_created/requests.hpp>
#include <views/api/internal/v1/shipments/ensure_created/responses.hpp>

namespace shipments::views::api::internal::v1::shipments::ensure_created {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace shipments::views::api::internal::v1::shipments::ensure_created
