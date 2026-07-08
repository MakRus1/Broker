#include <views/hello/view.hpp>

#include <greeting.hpp>

namespace broker::views::hello {

Response View::Handle(
    Request&& request,
    userver::server::request::RequestContext& /*context*/) {
    return PlainTextResponse{.body = SayHelloTo(request.name, UserType::kFirstTime)};
}

}  // namespace broker::views::hello
