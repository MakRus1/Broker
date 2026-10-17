#pragma once

#include <deps.hpp>

#include <views/hello/requests.hpp>
#include <views/hello/responses.hpp>

#include <userver/server/request/request_context.hpp>

namespace broker::views::hello {

class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context,
                           Deps deps);
};

}  // namespace broker::views::hello
