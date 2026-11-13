#include <components/document_parties.hpp>
#include <components/payments.hpp>
#include <converters/payment_input.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/payments/save/view.hpp>

namespace finance::views::api::v1::payments::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_PAYMENTS);

    const auto& body = request.body;
    auto value = converters::ToPayment(body);
    const auto& token = platform::auth::GetAccessToken(context);
    const auto parties = deps.document_parties->ResolveDocumentParties(token,
        {.OwnCompanyId = body.own_company_id,
            .CounterpartyId = body.counterparty_id,
            .OwnBankAccountId = body.own_bank_account_id,
            .CounterpartyBankAccountId = body.counterparty_bank_account_id,
            .ContractId = std::nullopt,
            .Currency = body.currency,
            .Archived = body.archived});
    value.OwnCompanyName = parties.OwnCompanyName;
    value.CounterpartyName = parties.CounterpartyName;

    auto result =
        deps.payments->SavePayment(organization, identity, request.body, std::move(value));
    return Response200{.allocated_minor = std::move(result.allocated_minor),
        .allocations = std::move(result.allocations),
        .payment = std::move(result.payment),
        .unallocated_minor = std::move(result.unallocated_minor)};
}
}  // namespace finance::views::api::v1::payments::save
