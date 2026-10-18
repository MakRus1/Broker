#pragma once

#include <deps.hpp>

#include <views/foo/bar/requests.hpp>
#include <views/foo/bar/responses.hpp>

#include <userver/server/request/request_context.hpp>

namespace broker::views::foo::bar {

class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};

}  // namespace broker::views::foo::bar
