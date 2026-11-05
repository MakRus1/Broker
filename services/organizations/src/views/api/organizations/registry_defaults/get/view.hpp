#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/organizations/registry_defaults/get/requests.hpp>
#include <views/api/organizations/registry_defaults/get/responses.hpp>

namespace organizations::views::api::organizations::registry_defaults::get {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace organizations::views::api::organizations::registry_defaults::get
