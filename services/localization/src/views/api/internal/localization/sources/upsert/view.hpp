#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/internal/localization/sources/upsert/requests.hpp>
#include <views/api/internal/localization/sources/upsert/responses.hpp>

namespace localization::views::api::internal::localization::sources::upsert {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace localization::views::api::internal::localization::sources::upsert
