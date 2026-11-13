#include <components/financial_history.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/service_charges/history/view.hpp>

namespace finance::views::api::v1::service_charges::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_SERVICE_CHARGES);

    const auto value = deps.history->GetHistory(organization, request.body,
        models::FinancialHistorySource::ServiceCharge,
        platform::auth::HasPermission(context, permissions::FINANCE_READ_INVOICES),
        platform::auth::HasPermission(context, permissions::FINANCE_READ_INVOICES));
    return Response200{.items = value.items, .next_before = value.next_before};
}
}  // namespace finance::views::api::v1::service_charges::history
