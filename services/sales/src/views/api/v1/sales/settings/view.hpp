#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/sales/settings/requests.hpp>
#include <views/api/v1/sales/settings/responses.hpp>

namespace sales::views::api::v1::sales::settings {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace sales::views::api::v1::sales::settings
