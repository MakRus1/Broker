#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/sales/proposals/action/requests.hpp>
#include <views/api/v1/sales/proposals/action/responses.hpp>

namespace web_bff::views::api::v1::sales::proposals::action {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::v1::sales::proposals::action
