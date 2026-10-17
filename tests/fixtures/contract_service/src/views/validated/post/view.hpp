#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/validated/post/requests.hpp>
#include <views/validated/post/responses.hpp>

namespace contract_service::views::validated::post {
class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context,
                           Deps);
};
}  // namespace contract_service::views::validated::post
