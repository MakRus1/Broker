#include <helpers/details.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/counterparties/update/view.hpp>

namespace counterparties::views::api::v1::counterparties::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        const auto organization = ResolveOrganization(*deps.pg_cluster, identity);
        const auto value = Prepare(request.body);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        // Порядок блокировок одинаков в создании и редактировании.
        CheckDuplicate(transaction, organization, value);
        const auto rows = transaction.Execute(sql::kLockCounterparty, organization, value.id);
        if (rows.IsEmpty())
            throw ApiError{404, "not_found", "Контрагент не найден"};
        const auto previous = Convert(rows.AsSingleRow<pgtypes::CounterpartyV2>());
        if (previous.revision != request.body.expected_revision)
            throw ApiError{409, "revision_conflict",
                "Контрагент изменился. Обновите карточку перед редактированием"};
        if (previous.archived)
            throw ApiError{409, "archived", "Сначала восстановите контрагента из архива"};
        const auto updated = transaction.Execute(sql::kUpdateCounterparty, organization, value.id,
            value.name, value.legal_name, value.country_code, value.tax_id, value.registration_id,
            value.legal_address, Convert(value.roles), Document(value.contacts));
        const auto result = Convert(updated.AsSingleRow<pgtypes::CounterpartyV2>());
        WriteAudit(transaction, organization, identity, "update_counterparty", result.id,
            request.body, result);
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
