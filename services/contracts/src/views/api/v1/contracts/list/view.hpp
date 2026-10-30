#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/contracts/list/requests.hpp>
#include <views/api/v1/contracts/list/responses.hpp>

namespace contracts::views::api::v1::contracts::list {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace contracts::views::api::v1::contracts::list
