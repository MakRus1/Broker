#include <views/items/by_id/view.hpp>
namespace contract_service::views::items::by_id {
Response View::Handle(Request&&, userver::server::request::RequestContext&, Deps) {
    return Response204{};
}
}  // namespace contract_service::views::items::by_id
