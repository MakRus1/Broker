#include <helpers/payments.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/payments/payment_candidates/view.hpp>

namespace finance::views::api::v1::payments::payment_candidates {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);
    const auto& body = request.body;
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto invoice = LoadInvoice(transaction, organization, body.invoice_id).invoice;
    if (invoice.archived)
        throw ApiError<409>{"invoice_archived", "Счёт находится в архиве"};
    const auto kind = invoice.direction == models::InvoiceDirection::Outgoing
        ? models::PaymentKind::Receipt
        : models::PaymentKind::Disbursement;
    const auto rows = transaction.Execute(sql::kPaymentCandidates, organization,
        models::EnumValue(kind), invoice.own_company_id, invoice.counterparty_id, invoice.currency,
        body.query, body.after, body.limit + 1);
    Response200 response;
    for (std::size_t index = 0; index < rows.Size() && index < static_cast<std::size_t>(body.limit);
         ++index) {
        const auto [payment, available] = rows[index].As<pgtypes::PaymentV1, std::int64_t>();
        response.items.push_back({.payment = Convert(payment), .unallocated_minor = available});
    }
    if (rows.Size() > static_cast<std::size_t>(body.limit))
        response.next_after = response.items.back().payment.id;
    transaction.Commit();
    return response;
}
}  // namespace finance::views::api::v1::payments::payment_candidates
