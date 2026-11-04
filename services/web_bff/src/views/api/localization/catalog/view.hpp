#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/localization/catalog/requests.hpp>
#include <views/api/localization/catalog/responses.hpp>

namespace web_bff::views::api::localization::catalog {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::localization::catalog
