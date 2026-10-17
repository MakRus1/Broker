#include <views/hello/get/view.hpp>

#include <domain/greeting.hpp>

namespace broker::views::hello::get {

Response View::Handle(Request&& request, userver::server::request::RequestContext& /*context*/,
                      Deps) {
    return Response200{
        .body = domain::SayHelloTo(request.name.value_or(""), domain::UserType::kFirstTime)};
}

}  // namespace broker::views::hello::get
