#include <components/invoices.hpp>
#include <utility>
#include <views/api/v1/invoices/detach_service_charge/view.hpp>

namespace finance::views::api::v1::invoices::detach_service_charge {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.invoices->DetachServiceCharge(request.body, context);
    return Response200{.charges = std::move(result.charges), .invoice = std::move(result.invoice)};
}
}  // namespace finance::views::api::v1::invoices::detach_service_charge
