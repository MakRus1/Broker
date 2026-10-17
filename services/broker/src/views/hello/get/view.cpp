#include <views/hello/get/view.hpp>

#include <greeting.hpp>

namespace broker::views::hello::get {

Response View::Handle(Request&& request, userver::server::request::RequestContext& /*context*/,
                      Deps) {
    return Response200{.body = SayHelloTo(request.name.value_or(""), UserType::kFirstTime)};
}

}  // namespace broker::views::hello::get
