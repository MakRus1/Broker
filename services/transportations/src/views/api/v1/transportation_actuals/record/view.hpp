#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/transportation_actuals/record/requests.hpp>
#include <views/api/v1/transportation_actuals/record/responses.hpp>

namespace transportations::views::api::v1::transportation_actuals::record {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace transportations::views::api::v1::transportation_actuals::record
