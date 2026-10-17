#include <views/foo/bar/get/view.hpp>

#include <fmt/format.h>

namespace broker::views::foo::bar::get {

Response View::Handle(Request&& request, userver::server::request::RequestContext& /*context*/,
                      Deps) {
    if (request.name == "missing") {
        return Response404{.message = "user not found"};
    }
    return Response200{.greeting = fmt::format("Hello, {}!", request.name)};
}

}  // namespace broker::views::foo::bar::get
