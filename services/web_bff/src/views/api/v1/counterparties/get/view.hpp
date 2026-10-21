#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/counterparties/get/requests.hpp>
#include <views/api/v1/counterparties/get/responses.hpp>

namespace web_bff::views::api::v1::counterparties::get {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::v1::counterparties::get
