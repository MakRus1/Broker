#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/internal/v1/service_charges/apply_import/requests.hpp>
#include <views/api/internal/v1/service_charges/apply_import/responses.hpp>

namespace finance::views::api::internal::v1::service_charges::apply_import {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace finance::views::api::internal::v1::service_charges::apply_import
