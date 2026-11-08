#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/sales/inquiries/history/requests.hpp>
#include <views/api/v1/sales/inquiries/history/responses.hpp>

namespace sales::views::api::v1::sales::inquiries::history {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace sales::views::api::v1::sales::inquiries::history
