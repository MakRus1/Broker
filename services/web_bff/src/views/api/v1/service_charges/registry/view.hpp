#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/service_charges/registry/requests.hpp>
#include <views/api/v1/service_charges/registry/responses.hpp>

namespace web_bff::views::api::v1::service_charges::registry {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::v1::service_charges::registry
