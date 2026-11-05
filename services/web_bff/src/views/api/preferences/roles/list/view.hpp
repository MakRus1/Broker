#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/preferences/roles/list/requests.hpp>
#include <views/api/preferences/roles/list/responses.hpp>

namespace web_bff::views::api::preferences::roles::list {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::preferences::roles::list
