#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/models/state/requests.hpp>
#include <views/models/state/responses.hpp>

namespace contract_service::views::models::state {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace contract_service::views::models::state
