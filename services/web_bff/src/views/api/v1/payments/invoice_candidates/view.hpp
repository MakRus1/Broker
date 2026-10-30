#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/payments/invoice_candidates/requests.hpp>
#include <views/api/v1/payments/invoice_candidates/responses.hpp>

namespace web_bff::views::api::v1::payments::invoice_candidates {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::v1::payments::invoice_candidates
