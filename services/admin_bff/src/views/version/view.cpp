#include <userver/server/handlers/exceptions.hpp>
#include <views/version/view.hpp>
namespace admin_bff::views::version {
Response View::Handle(Request&&, userver::server::request::RequestContext&, Deps) {
    return Response200{.body = "admin-bff/0.1.0\n"};
}
}  // namespace admin_bff::views::version
