#include <views/items/by_id/get/view.hpp>
namespace contract_service::views::items::by_id::get {
Response View::Handle(Request&&, userver::server::request::RequestContext&) {
    return Response204{};
}
}  // namespace contract_service::views::items::by_id::get
