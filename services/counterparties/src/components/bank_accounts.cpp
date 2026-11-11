#include <algorithm>
#include <cctype>
#include <components/bank_accounts.hpp>
#include <converters/bank_accounts.hpp>
#include <helpers/audit.hpp>
#include <helpers/normalization.hpp>
#include <limits>
#include <models/pg/bank_account_raw.hpp>
#include <models/pg/counterparty_raw.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace counterparties {
using namespace helpers;
using converters::Convert;

namespace {
void NormalizeAndValidate(models::BankAccount& value) {
    value.name = userver::utils::text::Trim(value.name);
    value.bank_name = userver::utils::text::Trim(value.bank_name);
    value.number = userver::utils::text::Trim(value.number);
    auto uppercase = [](std::string& text) {
        std::transform(text.begin(), text.end(), text.begin(),
            [](unsigned char c) { return std::toupper(c); });
    };
    std::erase(value.number, ' ');
    uppercase(value.number);
    uppercase(value.currency);
    for (auto* field : {&value.bic, &value.swift, &value.correspondent_account, &value.iban})
        field->emplace(Normalize(field->value_or(std::nullopt)));
    if (value.swift.value().has_value())
        uppercase(value.swift.value().value());
    if (value.iban.value().has_value()) {
        auto& iban = value.iban.value().value();
        std::erase(iban, ' ');
        uppercase(iban);
    }
    if (value.name.empty() || value.number.empty() || value.bank_name.empty())
        throw ApiError<400>{"empty_bank_requisites", "Укажите название счёта, номер и банк"};
    if (value.archived && value.is_primary)
        throw ApiError<400>{"archived_primary", "Архивный счёт не может быть основным"};
}
}  // namespace

BankAccountsComponent::BankAccountsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-counterparties")
                   .GetCluster()) {}

userver::yaml_config::Schema BankAccountsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::BankAccount BankAccountsComponent::Save(const models::SaveBankAccountRequest& request,
    userver::server::request::RequestContext& context) const {
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_WRITE_BANK_ACCOUNTS);
        const auto organization = platform::auth::GetOrganization(identity);
        if (request.expected_revision == std::numeric_limits<std::int64_t>::max())
            throw ApiError<400>{"invalid_revision", "Некорректная редакция счёта"};
        auto value = Convert(request);
        NormalizeAndValidate(value);
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

std::vector<models::BankAccount> BankAccountsComponent::List(
    const models::ListBankAccountsRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_READ_BANK_ACCOUNTS);
    const auto organization = platform::auth::GetOrganization(identity);
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
