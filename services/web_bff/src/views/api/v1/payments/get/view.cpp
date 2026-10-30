#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/payments/get/view.hpp>

namespace web_bff::views::api::v1::payments::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result =
        deps.finance.ApiV1PaymentsGet({.body = {.id = request.body.id}}, session.Access);
    return Response200{.allocated_minor = result.allocated_minor,
        .allocations = Convert(result.allocations),
        .payment = Convert(result.payment),
        .unallocated_minor = result.unallocated_minor};
}
}  // namespace web_bff::views::api::v1::payments::get
