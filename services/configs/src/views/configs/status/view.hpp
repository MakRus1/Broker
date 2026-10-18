#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/configs/status/requests.hpp>
#include <views/configs/status/responses.hpp>

namespace configs::views::configs::status {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace configs::views::configs::status
