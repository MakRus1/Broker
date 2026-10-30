#include <helpers/details.hpp>
#include <helpers/employees.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/counterparties/create/view.hpp>

namespace counterparties::views::api::v1::counterparties::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;

    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_WRITE);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto value = Prepare(request.body);
    if (request.body.logist.has_value())
        ValidateLogist(deps, context, request.body.logist.value());
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockCounterpartyIdentity, organization, value.id);
    CheckDuplicate(transaction, organization, value);
    const auto inserted =
        transaction.Execute(sql::kCreateCounterparty, organization, Convert(value));
    // UUID делает безопасным повтор создания после потери ответа.
    const auto rows = inserted.IsEmpty()
        ? transaction.Execute(sql::kGetCounterparty, organization, value.id)
        : inserted;
    const auto result = Convert(rows.AsSingleRow<pgtypes::CounterpartyV6>());
    if (Document(result) != Document(value))
        throw ApiError<409>{"id_conflict", "Идентификатор занят другим контрагентом"};
    if (!inserted.IsEmpty())
        WriteAudit(transaction, organization, identity, "create_counterparty", result.id,
            request.body, result);
    transaction.Commit();
    return Response201{.counterparty = result};
}
}  // namespace counterparties::views::api::v1::counterparties::create
