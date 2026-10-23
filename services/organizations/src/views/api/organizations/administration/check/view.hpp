#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/organizations/administration/check/requests.hpp>
#include <views/api/organizations/administration/check/responses.hpp>

namespace organizations::views::api::organizations::administration::check {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace organizations::views::api::organizations::administration::check
