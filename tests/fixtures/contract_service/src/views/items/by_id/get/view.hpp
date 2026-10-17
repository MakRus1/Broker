#pragma once
#include <userver/server/request/request_context.hpp>
#include <views/items/by_id/get/requests.hpp>
#include <views/items/by_id/get/responses.hpp>

namespace contract_service::views::items::by_id::get {
class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context);
};
}  // namespace contract_service::views::items::by_id::get
