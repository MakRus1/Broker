#include <helpers/payments.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/payments/invoice_balance/view.hpp>

namespace finance::views::api::v1::payments::invoice_balance {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto result = InvoiceBalance(transaction, organization, request.body.id);
    transaction.Commit();
    return Response200{.invoice = result.invoice,
        .outstanding_minor = result.outstanding_minor,
        .paid_minor = result.paid_minor,
        .payments = result.payments};
}
}  // namespace finance::views::api::v1::payments::invoice_balance
