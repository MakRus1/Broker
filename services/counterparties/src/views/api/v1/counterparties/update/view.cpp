#include <helpers/details.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/counterparties/update/view.hpp>

namespace counterparties::views::api::v1::counterparties::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_WRITE);
        const auto organization = platform::auth::GetOrganization(identity);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto id = boost::uuids::to_string(boost::uuids::string_generator{}(request.body.id));
        transaction.Execute(sql::kLockCounterpartyIdentity, organization, id);
        const auto rows = transaction.Execute(sql::kLockCounterparty, organization, id);
        if (rows.IsEmpty())
            throw ApiError{404, "not_found", "Контрагент не найден"};
        const auto previous = Convert(rows.AsSingleRow<pgtypes::CounterpartyV4>());
        if (previous.revision != request.body.expected_revision)
            throw ApiError{409, "revision_conflict",
                "Контрагент изменился. Обновите карточку перед редактированием"};
        if (previous.archived)
            throw ApiError{409, "archived", "Сначала восстановите контрагента из архива"};
        const auto value = Prepare(request.body, previous);
        CheckDuplicate(transaction, organization, value);
        const auto updated =
            transaction.Execute(sql::kUpdateCounterparty, organization, Convert(value));
        const auto result = Convert(updated.AsSingleRow<pgtypes::CounterpartyV4>());
        WriteAudit(transaction, organization, identity, "update_counterparty", result.id,
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
}  // namespace counterparties::views::api::v1::counterparties::update
