#include <views/composition/wrapped/view.hpp>

namespace contract_service::views::composition::wrapped {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps) {
    return Response200{.event = std::move(request.body.event),
        .events = std::move(request.body.events),
        .optional = std::move(request.body.optional)};
}
}  // namespace contract_service::views::composition::wrapped
