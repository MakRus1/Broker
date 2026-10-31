#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/admin/contracts/numbering/get/requests.hpp>
#include <views/api/admin/contracts/numbering/get/responses.hpp>

namespace contracts::views::api::admin::contracts::numbering::get {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace contracts::views::api::admin::contracts::numbering::get
