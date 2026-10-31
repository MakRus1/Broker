#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/exchange_rates/get/requests.hpp>
#include <views/api/v1/exchange_rates/get/responses.hpp>

namespace finance::views::api::v1::exchange_rates::get {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace finance::views::api::v1::exchange_rates::get
