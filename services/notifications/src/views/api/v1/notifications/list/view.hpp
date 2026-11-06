#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/notifications/list/requests.hpp>
#include <views/api/v1/notifications/list/responses.hpp>

namespace notifications::views::api::v1::notifications::list {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace notifications::views::api::v1::notifications::list
