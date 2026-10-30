#include <helpers/financial_entries.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/financial_entries/save/view.hpp>

namespace finance::views::api::v1::financial_entries::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_ENTRIES);
        const auto organization = platform::auth::GetOrganization(identity);
        ValidateEntryScope(deps, context,
            {.shipment_id = request.body.shipment_id,
                .transportation_id = request.body.transportation_id});
        const auto parties = ValidateEntryParties(deps, context, request.body);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto labels =
            transaction.Execute(sql::kGetFinancialLabel, organization, request.body.label_id);
        if (labels.IsEmpty())
            throw ApiError{404, "label_not_found", "Финансовая метка не найдена"};
        const auto value = PrepareEntry(
            request.body, Convert(labels.AsSingleRow<pgtypes::FinancialLabelV1>()), parties);
        const auto rows = request.body.expected_revision == 0
            ? transaction.Execute(sql::kCreateFinancialEntry, organization, value)
            : transaction.Execute(
                  sql::kUpdateFinancialEntry, organization, value, request.body.expected_revision);
        if (rows.IsEmpty())
            throw ApiError{409, "revision_conflict", "Начисление изменено. Обновите список"};
        const auto result = Convert(rows.AsSingleRow<pgtypes::FinancialEntryV1>());
        transaction.Execute(sql::kWriteEntryHistory, organization, result.id, result.revision,
            identity.ActorId(), userver::formats::json::ValueBuilder(result).ExtractValue());
        transaction.Commit();
        return Response200{.entry = result};
    } catch (const ::finance::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        case 409:
            return Response409{.error = error.Code, .message = error.what()};
        case 502:
            return Response502{.error = error.Code, .message = error.what()};
        default:
            throw;
        }
    }
}
}  // namespace finance::views::api::v1::financial_entries::save
