#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/transportation_actuals/correct/requests.hpp>
#include <views/api/v1/transportation_actuals/correct/responses.hpp>

namespace web_bff::views::api::v1::transportation_actuals::correct {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::v1::transportation_actuals::correct
