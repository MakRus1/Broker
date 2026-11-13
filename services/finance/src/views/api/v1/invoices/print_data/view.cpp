#include <components/document_parties.hpp>
#include <components/invoices.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/invoices/print_data/view.hpp>

namespace finance::views::api::v1::invoices::print_data {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);

    const auto snapshot = deps.invoices->ReadInvoicePrintSnapshot(organization, request.body);
    auto result = snapshot.Cached;
    if (!result) {
        const auto data = deps.document_parties->PrepareInvoicePrintData(
            platform::auth::GetAccessToken(context), snapshot.Details);
        result = deps.invoices->SaveInvoicePrintSnapshot(organization, data);
    }

    return Response200{.charges = std::move(result->charges),
        .customer = std::move(result->customer),
        .invoice = std::move(result->invoice),
        .supplier = std::move(result->supplier),
        .supplier_bank = std::move(result->supplier_bank)};
}
}  // namespace finance::views::api::v1::invoices::print_data
