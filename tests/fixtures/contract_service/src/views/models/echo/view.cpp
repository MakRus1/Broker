#include <views/models/echo/view.hpp>

namespace contract_service::views::models::echo {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps) {
    return Response200{.limit = request.body.limit,
                       .note = request.body.note,
                       .payload_provided = request.body.profile.WasProvided("state"),
                       .profile = std::move(request.body.profile),
                       .profiles = std::move(request.body.profiles)};
}
}  // namespace contract_service::views::models::echo
