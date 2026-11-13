#include <components/financial_labels.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/financial_labels/get/view.hpp>

namespace finance::views::api::v1::financial_labels::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_FINANCIAL_LABELS);

    return Response200{
        .label = deps.financial_labels->FindFinancialLabel(organization, request.body)};
}
}  // namespace finance::views::api::v1::financial_labels::get
