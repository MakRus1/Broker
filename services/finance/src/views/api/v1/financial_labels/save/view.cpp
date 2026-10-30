#include <helpers/financial_labels.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/financial_labels/save/view.hpp>

namespace finance::views::api::v1::financial_labels::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_FINANCIAL_LABELS);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto value = Prepare(request.body);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto rows = request.body.expected_revision == 0
        ? transaction.Execute(sql::kCreateFinancialLabel, organization, value)
        : transaction.Execute(
              sql::kUpdateFinancialLabel, organization, value, request.body.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Метка изменена. Обновите карточку"};
    const auto result = Convert(rows.AsSingleRow<pgtypes::FinancialLabelV1>());
    transaction.Execute(sql::kWriteHistory, organization, result.id, result.revision,
        identity.ActorId(), userver::formats::json::ValueBuilder(result).ExtractValue());
    transaction.Commit();
    return Response200{.label = result};
}
}  // namespace finance::views::api::v1::financial_labels::save
