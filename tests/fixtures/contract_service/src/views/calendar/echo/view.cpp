#include <views/calendar/echo/view.hpp>

namespace contract_service::views::calendar::echo {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps) {
    return Response200{.day = request.body.day,
        .days = request.body.days,
        .instant = request.body.instant,
        .optional_day = request.body.optional_day};
}
}  // namespace contract_service::views::calendar::echo
