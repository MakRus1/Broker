#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/sales/inquiries/get/requests.hpp>
#include <views/api/v1/sales/inquiries/get/responses.hpp>

namespace sales::views::api::v1::sales::inquiries::get {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace sales::views::api::v1::sales::inquiries::get
