#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/validated/create/requests.hpp>
#include <views/validated/create/responses.hpp>

namespace contract_service::views::validated::create {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps);
};
}  // namespace contract_service::views::validated::create
