#pragma once

#include <deps.hpp>
#include <views/hello_postgres/post/requests.hpp>
#include <views/hello_postgres/post/responses.hpp>

#include <userver/server/request/request_context.hpp>

namespace broker::views::hello_postgres::post {

class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context,
                           Deps deps);
};

}  // namespace broker::views::hello_postgres::post
