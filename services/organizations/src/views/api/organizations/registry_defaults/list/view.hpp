#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/organizations/registry_defaults/list/requests.hpp>
#include <views/api/organizations/registry_defaults/list/responses.hpp>

namespace organizations::views::api::organizations::registry_defaults::list {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace organizations::views::api::organizations::registry_defaults::list
