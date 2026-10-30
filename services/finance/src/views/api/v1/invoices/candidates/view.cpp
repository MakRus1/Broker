#include <helpers/invoices.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/invoices/candidates/view.hpp>

namespace finance::views::api::v1::invoices::candidates {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request.body;
    const auto rows =
        deps.pg_cluster->Execute(pg::ClusterHostType::kSlave, sql::kListInvoiceCandidates,
            organization, ServiceChargeDirection(body.direction), body.counterparty_id,
            body.currency, InvoiceId(body.contract_id), InvoiceId(body.invoice_id),
            InvoiceId(body.after), static_cast<std::int32_t>(body.limit + 1));
    auto items = ConvertServiceCharges(rows);
    std::optional<std::string> next;
    if (items.size() > static_cast<std::size_t>(body.limit)) {
        items.pop_back();
        next = items.back().id;
    }
    return Response200{.items = std::move(items), .next_after = next};
}
}  // namespace finance::views::api::v1::invoices::candidates
