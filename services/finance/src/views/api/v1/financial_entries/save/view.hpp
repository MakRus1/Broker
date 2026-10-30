#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/financial_entries/save/requests.hpp>
#include <views/api/v1/financial_entries/save/responses.hpp>

namespace finance::views::api::v1::financial_entries::save {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace finance::views::api::v1::financial_entries::save
