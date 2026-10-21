#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/counterparties/history/requests.hpp>
#include <views/api/v1/counterparties/history/responses.hpp>

namespace web_bff::views::api::v1::counterparties::history {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::v1::counterparties::history
