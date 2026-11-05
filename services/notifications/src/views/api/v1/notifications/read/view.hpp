#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/notifications/read/requests.hpp>
#include <views/api/v1/notifications/read/responses.hpp>

namespace notifications::views::api::v1::notifications::read {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace notifications::views::api::v1::notifications::read
