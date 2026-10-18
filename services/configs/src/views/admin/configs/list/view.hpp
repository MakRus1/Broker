#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/admin/configs/list/requests.hpp>
#include <views/admin/configs/list/responses.hpp>

namespace configs::views::admin::configs::list {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace configs::views::admin::configs::list
