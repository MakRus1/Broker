#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/counterparties/create/requests.hpp>
#include <views/api/v1/counterparties/create/responses.hpp>

namespace counterparties::views::api::v1::counterparties::create {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace counterparties::views::api::v1::counterparties::create
