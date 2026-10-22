#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/admin/organizations/create/requests.hpp>
#include <views/api/admin/organizations/create/responses.hpp>

namespace organizations::views::api::admin::organizations::create {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace organizations::views::api::admin::organizations::create
