#include <helpers/details.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/counterparties/set_archived/view.hpp>

namespace counterparties::views::api::v1::counterparties::set_archived {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;

    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_WRITE);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto rows = transaction.Execute(sql::kLockCounterparty, organization, request.body.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Контрагент не найден"};
    const auto previous = Convert(rows.AsSingleRow<pgtypes::CounterpartyV6>());
    if (previous.revision != request.body.expected_revision)
        throw ApiError<409>{"revision_conflict", "Контрагент изменился. Обновите карточку"};
    if (previous.archived == request.body.archived) {
        transaction.Commit();
        return Response200{.counterparty = previous};
    }
    const auto result = Convert(
        transaction.Execute(sql::kSetArchived, organization, request.body.id, request.body.archived)
            .AsSingleRow<pgtypes::CounterpartyV6>());
    WriteAudit(transaction, organization, identity,
        request.body.archived ? "archive_counterparty" : "restore_counterparty", result.id,
        request.body, result, std::optional{previous});
    transaction.Commit();
    return Response200{.counterparty = result};
}
}  // namespace counterparties::views::api::v1::counterparties::set_archived
