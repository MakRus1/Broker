#pragma once

#include <deps.hpp>

#include <views/hello/create/requests.hpp>
#include <views/hello/create/responses.hpp>

#include <userver/server/request/request_context.hpp>

namespace broker::views::hello::create {

class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context,
                           Deps deps);
};

}  // namespace broker::views::hello::create
