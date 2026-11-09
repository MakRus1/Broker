#include <components/inquiries.hpp>
#include <views/api/v1/sales/inquiries/list/view.hpp>

namespace sales::views::api::v1::sales::inquiries::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto result = deps.inquiries->List(request.body, context);
    return Response200{.items = result.items, .next_cursor = result.next_cursor};
}
}  // namespace sales::views::api::v1::sales::inquiries::list
