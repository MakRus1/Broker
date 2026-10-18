#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/admin/configs/drafts/list/requests.hpp>
#include <views/admin/configs/drafts/list/responses.hpp>

namespace configs::views::admin::configs::drafts::list {
class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context,
                           Deps deps);
};
}  // namespace configs::views::admin::configs::drafts::list
