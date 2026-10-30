#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/service_charges/save/requests.hpp>
#include <views/api/v1/service_charges/save/responses.hpp>

namespace web_bff::views::api::v1::service_charges::save {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::v1::service_charges::save
