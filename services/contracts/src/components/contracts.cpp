#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <components/contracts.hpp>
#include <contracts/sql_queries.hpp>
#include <converters/contracts.hpp>
#include <helpers/numbering.hpp>
#include <models/errors.hpp>
#include <models/pg/contract_raw.hpp>
#include <platform/auth/identity.hpp>
#include <platform/document_number.hpp>
#include <platform/http_client.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace models = contracts::models;
namespace pg = userver::storages::postgres;
namespace sql = contracts::sql;
using contracts::ApiError;
using contracts::helpers::ReadNumbering;

std::string GenerateContractNumber(pg::Transaction& transaction, const std::string& organization,
    userver::utils::datetime::Date date) {
    const auto settings = ReadNumbering(transaction.Execute(sql::kGetNumbering, organization));
    const auto sequence =
        transaction
            .Execute(sql::kNextDocumentNumber, organization, std::string{"contract"},
                platform::DocumentNumberPeriod(date, settings.reset_number_yearly))
            .AsSingleRow<std::int64_t>();
    const auto number = platform::FormatDocumentNumber(settings.number_template, date, sequence);
    if (number.size() > 128)
        throw ApiError<400>{"number_too_long", "Номер договора длиннее 128 символов"};
    return number;
}
}  // namespace

namespace contracts {
namespace pg = userver::storages::postgres;

ContractsComponent::ContractsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      counterparties_(context.FindComponent<clients::counterparties::Component>().GetClient()),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-contracts").GetCluster()) {
}

userver::yaml_config::Schema ContractsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

void ContractsComponent::ValidateContractParties(
    std::string_view token, const models::SaveContractRequest& value) const {
    try {
        for (const auto& id : {value.own_company_id, value.counterparty_id}) {
            const auto party =
                counterparties_.ApiV1CounterpartiesGet({.body = {.id = id}}, token).counterparty;
            if (party.archived)
                throw ApiError<409>{"archived_party", "Выберите действующие стороны договора"};
            if (id == value.own_company_id && !party.own_company)
                throw ApiError<400>{"not_own_company", "Выберите собственное юридическое лицо"};
            const auto account = id == value.own_company_id ? value.own_bank_account_id
                                                            : value.counterparty_bank_account_id;
            if (!account.has_value())
                continue;
            const auto accounts = counterparties_.ApiV1CounterpartiesBankAccountsList(
                {.body = {.counterparty_id = id}}, token);
            const auto found =
                std::find_if(accounts.items.begin(), accounts.items.end(), [&](const auto& item) {
                    return boost::uuids::string_generator{}(item.id) ==
                        boost::uuids::string_generator{}(account.value());
                });
            if (found == accounts.items.end())
                throw ApiError<400>{"invalid_account", "Счёт не принадлежит выбранной стороне"};
            if (found->archived || found->currency != value.currency)
                throw ApiError<400>{
                    "invalid_account", "Выберите действующий счёт в валюте договора"};
        }
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden_party", "Нет доступа к сторонам или счетам договора"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"party_not_found", "Сторона договора не найдена в организации"};
    } catch (const platform::http::ApiError&) {
        throw ApiError<502>{"counterparty_unavailable", "Не удалось проверить стороны договора"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError<502>{"counterparty_unavailable", "Справочник контрагентов недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<502>{
            "counterparty_unavailable", "Некорректный ответ справочника контрагентов"};
    }
}

models::Contract ContractsComponent::FindContract(
    const std::string& organization, const std::string& id) const {
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetContract, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Договор не найден"};
    return converters::Convert(rows.AsSingleRow<models::pg::ContractRaw>());
}

models::Contract ContractsComponent::SaveContract(const std::string& organization,
    const platform::auth::Identity& identity, std::int64_t expected_revision,
    models::pg::ContractRaw value) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    if (value.Number.empty())
        value.Number = GenerateContractNumber(transaction, organization, value.ContractDate);

    const auto rows = expected_revision == 0
        ? transaction.Execute(sql::kCreateContract, organization, value)
        : transaction.Execute(sql::kUpdateContract, organization, value, expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Договор изменён. Обновите карточку"};
    const auto result = converters::Convert(rows.AsSingleRow<models::pg::ContractRaw>());

    transaction.Execute(sql::kWriteHistory, organization, result.id, result.revision,
        identity.ActorId(), userver::formats::json::ValueBuilder(result).ExtractValue());
    transaction.Commit();

    return result;
}

}  // namespace contracts
