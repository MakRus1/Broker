#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/sales/cards/list/requests.hpp>
#include <views/api/v1/sales/cards/list/responses.hpp>

namespace sales::views::api::v1::sales::cards::list {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace sales::views::api::v1::sales::cards::list
