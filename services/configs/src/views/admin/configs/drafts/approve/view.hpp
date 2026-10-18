#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/admin/configs/drafts/approve/requests.hpp>
#include <views/admin/configs/drafts/approve/responses.hpp>

namespace configs::views::admin::configs::drafts::approve {
class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context,
                           Deps deps);
};
}  // namespace configs::views::admin::configs::drafts::approve
