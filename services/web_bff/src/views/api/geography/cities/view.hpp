#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/geography/cities/requests.hpp>
#include <views/api/geography/cities/responses.hpp>

namespace web_bff::views::api::geography::cities {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace web_bff::views::api::geography::cities
