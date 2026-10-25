#include <views/calendar/check/view.hpp>

namespace contract_service::views::calendar::check {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps) {
    return Response200{.day = request.day};
}
}  // namespace contract_service::views::calendar::check
