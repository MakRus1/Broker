#include <helpers/bank_accounts.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/counterparties/bank_accounts/list/view.hpp>

namespace counterparties::views::api::v1::counterparties::bank_accounts::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;

    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_READ_BANK_ACCOUNTS);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto owner = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kGetCounterparty,
        organization, request.body.counterparty_id);
    if (owner.IsEmpty())
        throw ApiError<404>{"not_found", "Юридическое лицо не найдено"};
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kListBankAccounts,
        organization, request.body.counterparty_id);
    return Response200{.items = ConvertBankAccounts(rows)};
}
}  // namespace counterparties::views::api::v1::counterparties::bank_accounts::list
