#include <components/payments.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/payments/invoice_balance/view.hpp>

namespace finance::views::api::v1::payments::invoice_balance {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);

    auto result = deps.payments->GetInvoiceBalance(organization, request.body);
    return Response200{.invoice = std::move(result.invoice),
        .outstanding_minor = std::move(result.outstanding_minor),
        .paid_minor = std::move(result.paid_minor),
        .payments = std::move(result.payments)};
}
}  // namespace finance::views::api::v1::payments::invoice_balance
