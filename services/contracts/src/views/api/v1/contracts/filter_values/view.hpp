#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/contracts/filter_values/requests.hpp>
#include <views/api/v1/contracts/filter_values/responses.hpp>

namespace contracts::views::api::v1::contracts::filter_values {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace contracts::views::api::v1::contracts::filter_values
