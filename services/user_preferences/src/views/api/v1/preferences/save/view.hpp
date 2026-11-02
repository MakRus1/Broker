#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/preferences/save/requests.hpp>
#include <views/api/v1/preferences/save/responses.hpp>

namespace user_preferences::views::api::v1::preferences::save {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace user_preferences::views::api::v1::preferences::save
