#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/admin/localization/save/requests.hpp>
#include <views/api/admin/localization/save/responses.hpp>

namespace admin_bff::views::api::admin::localization::save {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace admin_bff::views::api::admin::localization::save
