#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/invoices/detach_service_charge/requests.hpp>
#include <views/api/v1/invoices/detach_service_charge/responses.hpp>

namespace finance::views::api::v1::invoices::detach_service_charge {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace finance::views::api::v1::invoices::detach_service_charge
