#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/version/requests.hpp>
#include <views/version/responses.hpp>

namespace admin_bff::views::version {
class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context,
                           Deps deps);
};
}  // namespace admin_bff::views::version
