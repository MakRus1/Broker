#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/admin/finance/settings/get/requests.hpp>
#include <views/api/admin/finance/settings/get/responses.hpp>

namespace finance::views::api::admin::finance::settings::get {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace finance::views::api::admin::finance::settings::get
