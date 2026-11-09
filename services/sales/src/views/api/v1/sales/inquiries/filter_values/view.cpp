#include <components/inquiries.hpp>
#include <views/api/v1/sales/inquiries/filter_values/view.hpp>

namespace sales::views::api::v1::sales::inquiries::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto result = deps.inquiries->FilterValues(request.body, context);
    return Response200{
        .fields = result.fields, .next_after = result.next_after, .values = result.values};
}
}  // namespace sales::views::api::v1::sales::inquiries::filter_values
