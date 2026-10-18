#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/admin/configs/drafts/create/requests.hpp>
#include <views/admin/configs/drafts/create/responses.hpp>

namespace configs::views::admin::configs::drafts::create {
class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context,
                           Deps deps);
};
}  // namespace configs::views::admin::configs::drafts::create
