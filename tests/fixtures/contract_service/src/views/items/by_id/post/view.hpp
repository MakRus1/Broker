#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/items/by_id/post/requests.hpp>
#include <views/items/by_id/post/responses.hpp>

namespace contract_service::views::items::by_id::post {
class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context,
                           Deps);
};
}  // namespace contract_service::views::items::by_id::post
