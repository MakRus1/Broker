#include <components/invoices.hpp>
#include <utility>
#include <views/api/v1/invoices/save/view.hpp>

namespace finance::views::api::v1::invoices::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.invoices->Save(request.body, context);
    return Response200{.charges = std::move(result.charges), .invoice = std::move(result.invoice)};
}
}  // namespace finance::views::api::v1::invoices::save
