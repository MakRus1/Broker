#include <components/inquiries.hpp>
#include <views/api/v1/sales/inquiries/get/view.hpp>

namespace sales::views::api::v1::sales::inquiries::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.item = deps.inquiries->Get(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::inquiries::get
