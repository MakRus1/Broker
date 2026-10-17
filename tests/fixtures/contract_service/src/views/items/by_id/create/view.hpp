#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/items/by_id/create/requests.hpp>
#include <views/items/by_id/create/responses.hpp>

namespace contract_service::views::items::by_id::create {
class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context,
                           Deps);
};
}  // namespace contract_service::views::items::by_id::create
