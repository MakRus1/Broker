#include <components/payments.hpp>
#include <utility>
#include <views/api/v1/payments/get/view.hpp>

namespace finance::views::api::v1::payments::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.payments->Get(request.body, context);
    return Response200{.allocated_minor = std::move(result.allocated_minor),
        .allocations = std::move(result.allocations),
        .payment = std::move(result.payment),
        .unallocated_minor = std::move(result.unallocated_minor)};
}
}  // namespace finance::views::api::v1::payments::get
