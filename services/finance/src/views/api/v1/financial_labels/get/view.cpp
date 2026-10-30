#include <helpers/financial_labels.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/financial_labels/get/view.hpp>

namespace finance::views::api::v1::financial_labels::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_FINANCIAL_LABELS);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetFinancialLabel, organization, request.body.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Финансовая метка не найдена"};
    return Response200{.label = Convert(rows.AsSingleRow<pgtypes::FinancialLabelV1>())};
}
}  // namespace finance::views::api::v1::financial_labels::get
