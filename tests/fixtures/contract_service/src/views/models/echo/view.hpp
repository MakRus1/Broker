#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/models/echo/requests.hpp>
#include <views/models/echo/responses.hpp>

namespace contract_service::views::models::echo {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace contract_service::views::models::echo
