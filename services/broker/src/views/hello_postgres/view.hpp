#pragma once

#include <views/hello_postgres/deps.hpp>
#include <views/hello_postgres/requests.hpp>
#include <views/hello_postgres/responses.hpp>

#include <userver/server/request/request_context.hpp>

namespace broker::views::hello_postgres {

class View {
public:
    static Response Handle(
        Request&& request,
        userver::server::request::RequestContext& context,
        Deps deps);
};

}  // namespace broker::views::hello_postgres
