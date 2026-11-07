#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/admin/sales/settings/update/requests.hpp>
#include <views/api/admin/sales/settings/update/responses.hpp>

namespace admin_bff::views::api::admin::sales::settings::update {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace admin_bff::views::api::admin::sales::settings::update
