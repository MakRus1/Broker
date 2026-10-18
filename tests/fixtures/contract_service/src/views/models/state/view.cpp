#include <views/models/state/view.hpp>

namespace contract_service::views::models::state {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps) {
    return Response200{.state = request.state};
}
}  // namespace contract_service::views::models::state
