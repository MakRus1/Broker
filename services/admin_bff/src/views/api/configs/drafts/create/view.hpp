#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/configs/drafts/create/requests.hpp>
#include <views/api/configs/drafts/create/responses.hpp>

namespace admin_bff::views::api::configs::drafts::create {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace admin_bff::views::api::configs::drafts::create
