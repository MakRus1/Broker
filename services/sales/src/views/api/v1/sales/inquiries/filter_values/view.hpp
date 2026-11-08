#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/sales/inquiries/filter_values/requests.hpp>
#include <views/api/v1/sales/inquiries/filter_values/responses.hpp>

namespace sales::views::api::v1::sales::inquiries::filter_values {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace sales::views::api::v1::sales::inquiries::filter_values
