#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/organizations/me/requests.hpp>
#include <views/api/organizations/me/responses.hpp>

namespace admin_bff::views::api::organizations::me {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace admin_bff::views::api::organizations::me
