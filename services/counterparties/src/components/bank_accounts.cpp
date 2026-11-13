#include <components/bank_accounts.hpp>
#include <converters/bank_accounts.hpp>
#include <helpers/audit.hpp>
#include <helpers/bank_account_validation.hpp>
#include <limits>
#include <models/pg/bank_account_raw.hpp>
#include <models/pg/counterparty_raw.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace counterparties {
namespace pg = userver::storages::postgres;
using converters::Convert;
using helpers::WriteAudit;

BankAccountsComponent::BankAccountsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-counterparties")
                   .GetCluster()) {}

userver::yaml_config::Schema BankAccountsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::BankAccount BankAccountsComponent::SaveBankAccount(const std::string& organization,
    const platform::auth::Identity& identity, const models::SaveBankAccountRequest& request) const {
    try {
        if (request.expected_revision == std::numeric_limits<std::int64_t>::max())
            throw ApiError<400>{"invalid_revision", "Некорректная редакция счёта"};
        auto value = Convert(request);
        helpers::NormalizeAndValidateBankAccount(value);

        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        // Владелец сериализует изменения своих счетов и архивирование карточки.
        const auto owner =
            transaction.Execute(sql::kLockCounterparty, organization, value.counterparty_id);
        if (owner.IsEmpty())
            throw ApiError<404>{"not_found", "Юридическое лицо не найдено"};
        if (owner.AsSingleRow<models::pg::CounterpartyRaw>().Archived)
            throw ApiError<409>{
                "archived_owner", "Сначала восстановите юридическое лицо из архива"};
        const auto stored = transaction.Execute(sql::kGetBankAccount, organization, value.id);
        std::optional<models::BankAccount> previous;
        if (!stored.IsEmpty()) {
            previous = Convert(stored.AsSingleRow<models::pg::BankAccountRaw>());
            if (previous.value().counterparty_id != value.counterparty_id)
                throw ApiError<404>{"not_found", "Счёт не найден у этого юридического лица"};
            if (previous.value().revision != request.expected_revision)
                throw ApiError<409>{
                    "revision_conflict", "Счёт изменён. Обновите список перед редактированием"};
        } else {
            if (request.expected_revision != 0)
                throw ApiError<404>{"not_found", "Банковский счёт не найден"};
            if (transaction.Execute(sql::kListBankAccounts, organization, value.counterparty_id)
                    .Size() >= 100)
                throw ApiError<409>{
                    "account_limit", "У юридического лица уже 100 банковских счетов"};
        }

        const auto saved = transaction.Execute(
            previous.has_value() ? sql::kUpdateBankAccount : sql::kCreateBankAccount, organization,
            Convert(value));
        if (saved.IsEmpty())
            throw ApiError<409>{"id_conflict", "Идентификатор счёта уже занят. Обновите список"};
        const auto result = Convert(saved.AsSingleRow<models::pg::BankAccountRaw>());
        WriteAudit(transaction, organization, identity, "save_bank_account", result.id, request,
            result, previous);
        transaction.Commit();

        return result;
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{
            "account_conflict", "Уже есть такой номер или основной счёт в этой валюте"};
    }
}

std::vector<models::BankAccount> BankAccountsComponent::ListBankAccounts(
    const std::string& organization, const models::ListBankAccountsRequest& request) const {
    const auto owner = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetCounterparty, organization, request.counterparty_id);
    if (owner.IsEmpty())
        throw ApiError<404>{"not_found", "Юридическое лицо не найдено"};

    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kListBankAccounts,
        organization, request.counterparty_id);
    std::vector<models::BankAccount> items;
    items.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<models::pg::BankAccountRaw>())
        items.push_back(Convert(row));
    return items;
}
}  // namespace counterparties
