#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/sales/inquiries/save/requests.hpp>
#include <views/api/v1/sales/inquiries/save/responses.hpp>

namespace sales::views::api::v1::sales::inquiries::save {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace sales::views::api::v1::sales::inquiries::save
