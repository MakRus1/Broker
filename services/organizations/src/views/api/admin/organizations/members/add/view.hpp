#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/admin/organizations/members/add/requests.hpp>
#include <views/api/admin/organizations/members/add/responses.hpp>

namespace organizations::views::api::admin::organizations::members::add {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace organizations::views::api::admin::organizations::members::add
