#include <helpers/bank_accounts.hpp>
#include <permissions/permissions.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <views/api/v1/counterparties/bank_accounts/save/view.hpp>

namespace counterparties::views::api::v1::counterparties::bank_accounts::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_WRITE_BANK_ACCOUNTS);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto value = Prepare(request.body);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        // Владелец сериализует изменения своих счетов и архивирование карточки.
        const auto owner =
            transaction.Execute(sql::kLockCounterparty, organization, value.counterparty_id);
        if (owner.IsEmpty())
            throw ApiError{404, "not_found", "Юридическое лицо не найдено"};
        if (owner.AsSingleRow<pgtypes::CounterpartyV6>().Archived)
            throw ApiError{
                409, "archived_owner", "Сначала восстановите юридическое лицо из архива"};
        const auto stored = transaction.Execute(sql::kGetBankAccount, organization, value.id);
        std::optional<models::BankAccount> previous;
        if (!stored.IsEmpty()) {
            previous = Convert(stored.AsSingleRow<pgtypes::BankAccountV1>());
            if (previous.value().counterparty_id != value.counterparty_id)
                throw ApiError{404, "not_found", "Счёт не найден у этого юридического лица"};
            if (previous.value().revision != request.body.expected_revision)
                throw ApiError{409, "revision_conflict",
                    "Счёт изменён. Обновите список перед редактированием"};
        } else {
            if (request.body.expected_revision != 0)
                throw ApiError{404, "not_found", "Банковский счёт не найден"};
            if (transaction.Execute(sql::kListBankAccounts, organization, value.counterparty_id)
                    .Size() >= 100)
                throw ApiError{
                    409, "account_limit", "У юридического лица уже 100 банковских счетов"};
        }
        const auto saved = transaction.Execute(
            previous.has_value() ? sql::kUpdateBankAccount : sql::kCreateBankAccount, organization,
            Convert(value));
        if (saved.IsEmpty())
            throw ApiError{409, "id_conflict", "Идентификатор счёта уже занят. Обновите список"};
        const auto result = Convert(saved.AsSingleRow<pgtypes::BankAccountV1>());
        WriteAudit(transaction, organization, identity, "save_bank_account", result.id,
            request.body, result, previous);
        transaction.Commit();
        return Response200{.account = result};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{.error = "account_conflict",
            .message = "Уже есть такой номер или основной счёт в этой валюте"};
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
}  // namespace counterparties::views::api::v1::counterparties::bank_accounts::save
