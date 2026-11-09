#include <components/service_charges.hpp>
#include <utility>
#include <views/api/v1/service_charges/registry/view.hpp>

namespace finance::views::api::v1::service_charges::registry {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.service_charges->Registry(request.body, context);
    return Response200{.invoices = std::move(result.invoices),
        .items = std::move(result.items),
        .next_cursor = std::move(result.next_cursor)};
}
}  // namespace finance::views::api::v1::service_charges::registry
