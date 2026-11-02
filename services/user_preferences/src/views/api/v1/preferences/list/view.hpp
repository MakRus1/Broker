#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/preferences/list/requests.hpp>
#include <views/api/v1/preferences/list/responses.hpp>

namespace user_preferences::views::api::v1::preferences::list {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace user_preferences::views::api::v1::preferences::list
