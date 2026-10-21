#include <helpers/details.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/counterparties/set_archived/view.hpp>

namespace counterparties::views::api::v1::counterparties::set_archived {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        const auto organization = platform::auth::GetOrganization(identity);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto rows =
            transaction.Execute(sql::kLockCounterparty, organization, request.body.id);
        if (rows.IsEmpty())
            throw ApiError{404, "not_found", "Контрагент не найден"};
        const auto previous = Convert(rows.AsSingleRow<pgtypes::CounterpartyV3>());
        if (previous.revision != request.body.expected_revision)
            throw ApiError{409, "revision_conflict", "Контрагент изменился. Обновите карточку"};
        if (previous.archived == request.body.archived) {
            transaction.Commit();
            return Response200{.counterparty = previous};
        }
        const auto result = Convert(
            transaction
                .Execute(sql::kSetArchived, organization, request.body.id, request.body.archived)
                .AsSingleRow<pgtypes::CounterpartyV3>());
        WriteAudit(transaction, organization, identity,
            request.body.archived ? "archive_counterparty" : "restore_counterparty", result.id,
            request.body, result, std::optional{previous});
        transaction.Commit();
        return Response200{.counterparty = result};
    } catch (const ::counterparties::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        case 409:
            return Response409{.error = error.Code, .message = error.what()};
        default:
            throw;
        }
    }
}
}  // namespace counterparties::views::api::v1::counterparties::set_archived
