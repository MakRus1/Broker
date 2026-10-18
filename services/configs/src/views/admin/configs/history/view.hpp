#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/admin/configs/history/requests.hpp>
#include <views/admin/configs/history/responses.hpp>

namespace configs::views::admin::configs::history {
class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context,
                           Deps deps);
};
}  // namespace configs::views::admin::configs::history
