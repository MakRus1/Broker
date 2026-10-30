#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/payments/invoice_balance/requests.hpp>
#include <views/api/v1/payments/invoice_balance/responses.hpp>

namespace finance::views::api::v1::payments::invoice_balance {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace finance::views::api::v1::payments::invoice_balance
