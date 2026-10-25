#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/calendar/echo/requests.hpp>
#include <views/calendar/echo/responses.hpp>

namespace contract_service::views::calendar::echo {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace contract_service::views::calendar::echo
