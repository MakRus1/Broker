#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/configs/values/requests.hpp>
#include <views/configs/values/responses.hpp>

namespace configs::views::configs::values {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace configs::views::configs::values
