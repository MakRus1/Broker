#pragma once

#include <deps.hpp>

#include <views/foo/bar/get/requests.hpp>
#include <views/foo/bar/get/responses.hpp>

#include <userver/server/request/request_context.hpp>

namespace broker::views::foo::bar::get {

class View {
  public:
    static Response Handle(Request&& request, userver::server::request::RequestContext& context,
                           Deps deps);
};

}  // namespace broker::views::foo::bar::get
