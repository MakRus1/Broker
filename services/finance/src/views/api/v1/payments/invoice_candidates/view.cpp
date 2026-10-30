#include <helpers/payments.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/payments/invoice_candidates/view.hpp>

namespace finance::views::api::v1::payments::invoice_candidates {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request.body;
    const auto rows =
        deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kPaymentInvoiceCandidates,
            organization, models::EnumValue(PaymentInvoiceDirection(body.kind)),
            body.own_company_id, body.counterparty_id, body.currency, body.query, body.after,
            body.limit + 1, body.payment_id);
    Response200 result;
    for (std::size_t index = 0; index < rows.Size() && index < static_cast<std::size_t>(body.limit);
         ++index) {
        const auto [row, paid] = rows[index].As<pgtypes::InvoiceV1, std::int64_t>();
        const auto invoice = Convert(row);
        result.items.push_back({.invoice = invoice,
            .outstanding_minor = invoice.total_minor - paid,
            .paid_minor = paid,
            .payments = {}});
    }
    if (rows.Size() > static_cast<std::size_t>(body.limit))
        result.next_after = result.items.back().invoice.id;
    return result;
}
}  // namespace finance::views::api::v1::payments::invoice_candidates
