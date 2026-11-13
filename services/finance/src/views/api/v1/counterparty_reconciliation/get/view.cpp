#include <components/settlements.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/counterparty_reconciliation/get/view.hpp>

namespace finance::views::api::v1::counterparty_reconciliation::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);

    auto result = deps.settlements->GetReconciliation(organization, request.body);
    return Response200{
        .operations = std::move(result.operations), .totals = std::move(result.totals)};
}
}  // namespace finance::views::api::v1::counterparty_reconciliation::get
