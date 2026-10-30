#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/financial_entries/list/requests.hpp>
#include <views/api/v1/financial_entries/list/responses.hpp>

namespace web_bff::views::api::v1::financial_entries::list {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::v1::financial_entries::list
