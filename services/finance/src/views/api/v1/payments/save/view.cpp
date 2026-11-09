#include <components/payments.hpp>
#include <utility>
#include <views/api/v1/payments/save/view.hpp>

namespace finance::views::api::v1::payments::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.payments->Save(request.body, context);
    return Response200{.allocated_minor = std::move(result.allocated_minor),
        .allocations = std::move(result.allocations),
        .payment = std::move(result.payment),
        .unallocated_minor = std::move(result.unallocated_minor)};
}
}  // namespace finance::views::api::v1::payments::save
