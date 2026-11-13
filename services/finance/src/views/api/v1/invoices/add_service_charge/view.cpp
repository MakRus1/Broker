#include <components/invoices.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/invoices/add_service_charge/view.hpp>

namespace finance::views::api::v1::invoices::add_service_charge {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_INVOICES);

    auto result = deps.invoices->IncludeServiceCharge(organization, identity, request.body);
    return Response200{.charges = std::move(result.charges), .invoice = std::move(result.invoice)};
}
}  // namespace finance::views::api::v1::invoices::add_service_charge
