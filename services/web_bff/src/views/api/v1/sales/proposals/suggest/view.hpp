#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/sales/proposals/suggest/requests.hpp>
#include <views/api/v1/sales/proposals/suggest/responses.hpp>

namespace web_bff::views::api::v1::sales::proposals::suggest {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::v1::sales::proposals::suggest
