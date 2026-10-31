#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/admin/contracts/numbering/update/requests.hpp>
#include <views/api/admin/contracts/numbering/update/responses.hpp>

namespace admin_bff::views::api::admin::contracts::numbering::update {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace admin_bff::views::api::admin::contracts::numbering::update
