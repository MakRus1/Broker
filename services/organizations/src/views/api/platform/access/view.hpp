#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/platform/access/requests.hpp>
#include <views/api/platform/access/responses.hpp>

namespace organizations::views::api::platform::access {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace organizations::views::api::platform::access
