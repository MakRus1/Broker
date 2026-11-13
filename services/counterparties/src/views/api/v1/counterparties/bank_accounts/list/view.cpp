#include <components/bank_accounts.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/counterparties/bank_accounts/list/view.hpp>

namespace counterparties::views::api::v1::counterparties::bank_accounts::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_READ_BANK_ACCOUNTS);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);

    auto result = deps.bank_accounts->ListBankAccounts(organization, request.body);

    return Response200{.items = std::move(result)};
}
}  // namespace counterparties::views::api::v1::counterparties::bank_accounts::list
