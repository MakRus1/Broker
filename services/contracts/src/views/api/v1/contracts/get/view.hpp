#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/contracts/get/requests.hpp>
#include <views/api/v1/contracts/get/responses.hpp>

namespace contracts::views::api::v1::contracts::get {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace contracts::views::api::v1::contracts::get
