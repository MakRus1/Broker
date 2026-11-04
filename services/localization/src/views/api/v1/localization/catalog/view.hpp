#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/localization/catalog/requests.hpp>
#include <views/api/v1/localization/catalog/responses.hpp>

namespace localization::views::api::v1::localization::catalog {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace localization::views::api::v1::localization::catalog
