#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <components/contracts.hpp>
#include <contracts/sql_queries.hpp>
#include <converters/contracts.hpp>
#include <helpers/numbering.hpp>
#include <limits>
#include <models/errors.hpp>
#include <models/pg/contract_raw.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/date_validation.hpp>
#include <platform/document_number.hpp>
#include <platform/http_client.hpp>
#include <platform/registry_page.hpp>
#include <registry/fields.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/utils/text_light.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace contracts {
namespace pg = userver::storages::postgres;
using helpers::ReadNumbering;

namespace {
void NormalizeAndValidate(models::SaveContractRequest& value) {
    if (value.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция договора"};
    const auto number = userver::utils::text::Trim(value.number.value_or(""));
    if (number.empty() && (value.number.has_value() || value.expected_revision != 0))
        throw ApiError<400>{"empty_number", "Укажите номер договора"};
    for (const auto& date : {std::optional{value.contract_date}, value.valid_until}) {
        if (date.has_value() &&
            !platform::IsCalendarDateInRange(
                date.value(), std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
            throw ApiError<400>{"invalid_date", "Дата должна быть не раньше 01.01.1900"};
    }
    if (value.valid_until.has_value() && value.valid_until.value() < value.contract_date)
        throw ApiError<400>{
            "invalid_valid_until", "Срок действия не может быть раньше даты договора"};
    const auto own = boost::uuids::string_generator{}(value.own_company_id);
    const auto counterparty = boost::uuids::string_generator{}(value.counterparty_id);
    if (own == counterparty)
        throw ApiError<400>{"same_parties", "Выберите разные стороны договора"};
    value.number = number;
    if (value.note.has_value()) {
        value.note = userver::utils::text::Trim(value.note.value());
        if (value.note.value().empty())
            value.note.reset();
    }
}

struct RegistryRow {
    models::pg::ContractRaw Contract;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};

registry::QueryBuilder BuildRegistryFilters(const models::ListContractsRequest& request,
    const std::string& organization, const std::optional<std::string>& exclude = std::nullopt) {
    using namespace registry_fields;
    registry::QueryBuilder query{
        sql::kListContracts, registry::RegistryOrderBy(kFields, request.sort), request.descending};
    query.WhereEqual(kOrganization, organization);
    registry::ApplyFilters(query, kFields, request.filters, exclude);
    return query;
}

const registry::Definition kRegistry{
    registry_fields::kFields, sql::kRegistryFilterValues, &BuildRegistryFilters};

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

void ContractsComponent::ValidateParties(const userver::server::request::RequestContext& context,
    const models::SaveContractRequest& value) const {
    const auto token = platform::auth::GetAccessToken(context);
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

models::Contract ContractsComponent::Get(
    const std::string& id, userver::server::request::RequestContext& context) const {

    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::CONTRACTS_READ);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetContract, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Договор не найден"};
    return converters::Convert(rows.AsSingleRow<models::pg::ContractRaw>());
}

models::Contract ContractsComponent::Save(const models::SaveContractRequest& request,
    userver::server::request::RequestContext& context) const {

    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::CONTRACTS_WRITE);
    const auto organization = platform::auth::GetOrganization(identity);
    auto normalized = request;
    NormalizeAndValidate(normalized);
    auto value = converters::Convert(normalized);
    ValidateParties(context, request);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    if (value.Number.empty())
        value.Number = GenerateContractNumber(transaction, organization, value.ContractDate);
    const auto rows = request.expected_revision == 0
        ? transaction.Execute(sql::kCreateContract, organization, value)
        : transaction.Execute(sql::kUpdateContract, organization, value, request.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Договор изменён. Обновите карточку"};
    const auto result = converters::Convert(rows.AsSingleRow<models::pg::ContractRaw>());
    transaction.Execute(sql::kWriteHistory, organization, result.id, result.revision,
        identity.ActorId(), userver::formats::json::ValueBuilder(result).ExtractValue());
    transaction.Commit();
    return result;
}

ContractsComponent::ListResult ContractsComponent::List(const models::ListContractsRequest& request,
    userver::server::request::RequestContext& context) const {
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::CONTRACTS_READ);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto& body = request;
        const auto scope = platform::registry::CursorScope(body, organization);
        const auto order = registry::RegistryOrderBy(registry_fields::kFields, body.sort);
        const bool date_sort = order.CursorType == registry::CursorKeyType::Date;
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto query = platform::registry::BuildPage(kRegistry, body, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
            pg::TransactionOptions{
                pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
        transaction.Execute(sql::kRegistryPlan);
        auto rows = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<RegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        std::optional<models::ContractRegistryCursor> cursor;
        if (more) {
            const auto& last = rows.back();
            cursor = models::ContractRegistryCursor{
                .date_value = date_sort ? last.DateKey : std::nullopt,
                .id = boost::uuids::to_string(last.Contract.Id),
                .scope = scope,
                .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        transaction.Commit();
        std::vector<models::Contract> items;
        items.reserve(rows.size());
        for (const auto& row : rows)
            items.push_back(converters::Convert(row.Contract));
        return ListResult{.items = std::move(items), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

ContractsComponent::FilterValuesResult ContractsComponent::FilterValues(
    const models::ContractFilterValuesRequest& request,
    userver::server::request::RequestContext& context) const {
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        FilterValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(registry_fields::kFields);
        if (!request.field.has_value())
            return result;
        const auto query = platform::registry::BuildFilterValues(kRegistry, request, organization);
        const auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(request.limit);
        for (std::size_t index = 0;
             index < rows.size() && index < static_cast<std::size_t>(request.limit); ++index)
            result.values.push_back({.label = rows[index].Label, .value = rows[index].Value});
        if (more)
            result.next_after = result.values.back().value;
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}
}  // namespace contracts
