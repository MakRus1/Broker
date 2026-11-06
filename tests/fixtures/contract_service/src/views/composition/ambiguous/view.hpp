#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/composition/ambiguous/requests.hpp>
#include <views/composition/ambiguous/responses.hpp>

namespace contract_service::views::composition::ambiguous {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace contract_service::views::composition::ambiguous
