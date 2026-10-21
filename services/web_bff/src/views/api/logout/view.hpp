#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/logout/requests.hpp>
#include <views/api/logout/responses.hpp>

namespace web_bff::views::api::logout {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::logout
