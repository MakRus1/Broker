#include <components/service_charges.hpp>
#include <utility>
#include <views/api/v1/service_charges/list/view.hpp>

namespace finance::views::api::v1::service_charges::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.service_charges->List(request.body, context);
    return Response200{.invoices = std::move(result.invoices),
        .items = std::move(result.items),
        .next_after = std::move(result.next_after),
        .reporting = std::move(result.reporting),
        .summary = std::move(result.summary)};
}
}  // namespace finance::views::api::v1::service_charges::list
