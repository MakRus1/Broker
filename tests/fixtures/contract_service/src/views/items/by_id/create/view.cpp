#include <views/items/by_id/create/view.hpp>

namespace contract_service::views::items::by_id::create {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps) {
    if (request.body.count && *request.body.count < 0)
        return Response409{.id = request.id};
    return Response201{.id = request.id};
}
}  // namespace contract_service::views::items::by_id::create
