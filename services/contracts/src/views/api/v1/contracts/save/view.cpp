#include <helpers/contracts.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/contracts/save/view.hpp>

namespace contracts::views::api::v1::contracts::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::contracts::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::CONTRACTS_WRITE);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto value = Prepare(request.body);
        ValidateParties(deps, context, request.body);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto rows = request.body.expected_revision == 0
            ? transaction.Execute(sql::kCreateContract, organization, value)
            : transaction.Execute(
                  sql::kUpdateContract, organization, value, request.body.expected_revision);
        if (rows.IsEmpty())
            throw ApiError{409, "revision_conflict", "Договор изменён. Обновите карточку"};
        const auto result = Convert(rows.AsSingleRow<pgtypes::ContractV1>());
        transaction.Execute(sql::kWriteHistory, organization, result.id, result.revision,
            identity.ActorId(), userver::formats::json::ValueBuilder(result).ExtractValue());
        transaction.Commit();
        return Response200{.contract = result};
    } catch (const ::contracts::ApiError& error) {
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
}  // namespace contracts::views::api::v1::contracts::save
