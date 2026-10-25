#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/calendar/check/requests.hpp>
#include <views/calendar/check/responses.hpp>

namespace contract_service::views::calendar::check {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace contract_service::views::calendar::check
