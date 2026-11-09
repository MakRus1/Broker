#include <components/invoices.hpp>
#include <utility>
#include <views/api/v1/invoices/print_data/view.hpp>

namespace finance::views::api::v1::invoices::print_data {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.invoices->PrintData(request.body, context);
    return Response200{.charges = std::move(result.charges),
        .customer = std::move(result.customer),
        .invoice = std::move(result.invoice),
        .supplier = std::move(result.supplier),
        .supplier_bank = std::move(result.supplier_bank)};
}
}  // namespace finance::views::api::v1::invoices::print_data
