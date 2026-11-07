#include <helpers/registry.hpp>
#include <views/api/v1/sales/inquiries/list/view.hpp>

namespace sales::views::api::v1::sales::inquiries::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    try {
        auto page =
            List<models::Inquiry>(deps, kInquiriesRegistry, request.body, Organization(context));
        return Response200{.items = std::move(page.Items), .next_cursor = page.Cursor};
    } catch (const registry::FilterError& error) {
        return Response400{.error = "invalid_filter", .message = error.what()};
    }
}
}  // namespace sales::views::api::v1::sales::inquiries::list
