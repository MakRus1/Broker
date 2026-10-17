#pragma once
#include <userver/server/request/request_context.hpp>
#include <views/validated/get/requests.hpp>
#include <views/validated/get/responses.hpp>

namespace contract_service::views::validated::get {
class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context);
};
}  // namespace contract_service::views::validated::get
