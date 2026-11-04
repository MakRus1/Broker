#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/internal/permissions/releases/upsert/requests.hpp>
#include <views/api/internal/permissions/releases/upsert/responses.hpp>

namespace organizations::views::api::internal::permissions::releases::upsert {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace organizations::views::api::internal::permissions::releases::upsert
