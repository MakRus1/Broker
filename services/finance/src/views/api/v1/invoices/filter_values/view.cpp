#include <components/invoices.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/invoices/filter_values/view.hpp>

namespace finance::views::api::v1::invoices::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);

    auto result = deps.invoices->GetInvoiceFilterValues(organization, request.body);
    return Response200{.fields = std::move(result.fields),
        .next_after = std::move(result.next_after),
        .values = std::move(result.values)};
}
}  // namespace finance::views::api::v1::invoices::filter_values
