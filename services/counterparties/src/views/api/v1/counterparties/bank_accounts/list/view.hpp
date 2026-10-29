#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/counterparties/bank_accounts/list/requests.hpp>
#include <views/api/v1/counterparties/bank_accounts/list/responses.hpp>

namespace counterparties::views::api::v1::counterparties::bank_accounts::list {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace counterparties::views::api::v1::counterparties::bank_accounts::list
