#include <components/inquiries.hpp>
#include <views/api/v1/sales/inquiries/history/view.hpp>

namespace sales::views::api::v1::sales::inquiries::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.items = deps.inquiries->History(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::inquiries::history
