#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/organizations/registry_defaults/save/requests.hpp>
#include <views/api/organizations/registry_defaults/save/responses.hpp>

namespace organizations::views::api::organizations::registry_defaults::save {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace organizations::views::api::organizations::registry_defaults::save
