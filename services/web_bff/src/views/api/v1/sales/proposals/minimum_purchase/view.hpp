#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/sales/proposals/minimum_purchase/requests.hpp>
#include <views/api/v1/sales/proposals/minimum_purchase/responses.hpp>

namespace web_bff::views::api::v1::sales::proposals::minimum_purchase {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::v1::sales::proposals::minimum_purchase
