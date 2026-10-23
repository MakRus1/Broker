#include <helpers/details.hpp>
#include <helpers/employees.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/counterparties/create/view.hpp>

namespace counterparties::views::api::v1::counterparties::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;
    try {
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
        const auto result = Convert(rows.AsSingleRow<pgtypes::CounterpartyV5>());
        if (Document(result) != Document(value))
            throw ApiError{409, "id_conflict", "Идентификатор занят другим контрагентом"};
        if (!inserted.IsEmpty())
            WriteAudit(transaction, organization, identity, "create_counterparty", result.id,
                request.body, result);
        transaction.Commit();
        return Response201{.counterparty = result};
    } catch (const ::counterparties::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        case 503:
            return Response503{.error = error.Code, .message = error.what()};
        case 409:
            return Response409{.error = error.Code, .message = error.what()};
        default:
            throw;
        }
    }
}
}  // namespace counterparties::views::api::v1::counterparties::create
