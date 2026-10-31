#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/admin/finance/settings/update/requests.hpp>
#include <views/api/admin/finance/settings/update/responses.hpp>

namespace admin_bff::views::api::admin::finance::settings::update {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace admin_bff::views::api::admin::finance::settings::update
