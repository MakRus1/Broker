#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/notifications/mark_read/requests.hpp>
#include <views/api/notifications/mark_read/responses.hpp>

namespace web_bff::views::api::notifications::mark_read {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::notifications::mark_read
