#include <components/document_parties.hpp>
#include <components/invoices.hpp>
#include <converters/invoice_input.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/invoices/save/view.hpp>

namespace finance::views::api::v1::invoices::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_INVOICES);

    const auto& body = request.body;
    auto value = converters::ToInvoice(body);
    const auto& token = platform::auth::GetAccessToken(context);
    const auto parties = deps.document_parties->ResolveDocumentParties(token,
        {.OwnCompanyId = body.own_company_id,
            .CounterpartyId = body.counterparty_id,
            .OwnBankAccountId = body.own_bank_account_id,
            .CounterpartyBankAccountId = body.counterparty_bank_account_id,
            .ContractId = body.contract_id,
            .Currency = body.currency,
            .Archived = body.archived});
    value.OwnCompanyName = parties.OwnCompanyName;
    value.CounterpartyName = parties.CounterpartyName;
    value.ContractNumber = parties.ContractNumber;

    auto result =
        deps.invoices->SaveInvoice(organization, identity, request.body, std::move(value));
    return Response200{.charges = std::move(result.charges), .invoice = std::move(result.invoice)};
}
}  // namespace finance::views::api::v1::invoices::save
