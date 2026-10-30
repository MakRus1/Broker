#pragma once
#include <helpers/registry.hpp>
#include <platform/registry_filters.hpp>

namespace contracts::helpers {
namespace registry = platform::registry;

namespace registry_fields {

// Поля SQL и каталог API определяются здесь; общий конструктор добавляет фильтры и курсор.
using Date = userver::utils::datetime::Date;
inline constexpr registry::Field<std::string> kId{"id", "id", "UUID", true};
inline constexpr registry::Field<std::string> kOrganization{
    "organization_id", "organization_id", "UUID"};
inline constexpr auto kNumber =
    registry::Field<std::string>{"number", "lower(number) COLLATE \"C\""}.SortedAs(
        "lower(number) COLLATE public.registry_number_order");
inline constexpr registry::Field<std::string> kOwnCompany{
    "own_company_id", "own_company_id", "UUID"};
inline constexpr registry::Field<std::string> kCounterparty{
    "counterparty_id", "counterparty_id", "UUID"};
inline constexpr registry::Field<Date> kDate{"contract_date", "contract_date", "DATE"};
inline constexpr auto kValidUntil =
    registry::Field<Date>{"valid_until", "valid_until", "DATE"}.SortedAs(
        "COALESCE(valid_until, DATE '9999-12-31')");
inline constexpr registry::Field<std::string> kCurrency{"currency", "currency"};
inline constexpr auto kArchived = registry::Field<bool>{"archived", "archived", "BOOLEAN"}.SortedAs(
    "(CASE WHEN archived THEN '1' ELSE '0' END) COLLATE \"C\"");
inline constexpr std::array<registry::RegistryField, 8> kFields{{
    {.Field = kId, .Label = "Идентификатор", .ValueSql = "id", .Visible = false},
    {.Field = kNumber, .Label = "Номер договора", .ValueSql = "number", .FoldValues = true},
    {.Field = kOwnCompany,
        .Label = "Собственное юрлицо",
        .ValueSql = "own_company_id",
        .Sortable = false,
        .Lookup = "counterparties"},
    {.Field = kCounterparty,
        .Label = "Контрагент",
        .ValueSql = "counterparty_id",
        .Sortable = false,
        .Lookup = "counterparties"},
    {.Field = kDate, .Label = "Дата договора", .ValueSql = "contract_date"},
    {.Field = kValidUntil, .Label = "Действует до", .ValueSql = "valid_until"},
    {.Field = kCurrency, .Label = "Валюта", .ValueSql = "currency"},
    {.Field = kArchived, .Label = "Состояние", .ValueSql = "archived"},
}};
inline constexpr auto kSortableFields = registry::SortableFields(kFields);
}  // namespace registry_fields

// Общие условия страницы и списка вариантов: организация и применённые фильтры.
inline registry::QueryBuilder BuildRegistryFilters(const models::ListContractsRequest& request,
    const std::string& organization, const std::optional<std::string>& exclude = std::nullopt) {
    using namespace registry_fields;
    registry::QueryBuilder query{
        sql::kListContracts, registry::RegistryOrderBy(kFields, request.sort), request.descending};
    query.WhereEqual(kOrganization, organization);
    registry::ApplyFilters(query, kFields, request.filters, exclude);
    return query;
}

// Страница реестра: общие условия + курсор + лимит с одной дополнительной записью.
inline registry::BuiltQuery BuildRegistryQuery(
    const models::ListContractsRequest& request, const std::string& organization) {
    using namespace registry_fields;
    auto query = BuildRegistryFilters(request, organization);
    const auto order = registry::RegistryOrderBy(kFields, request.sort);
    const auto after =
        request.cursor.has_value() ? std::optional{request.cursor.value().id} : std::nullopt;
    if (after.has_value() && order.CursorType == registry::CursorKeyType::Date)
        query.After(after, request.cursor.value().date_value.value());
    else
        query.After(after,
            request.cursor.has_value() ? request.cursor.value().text_value.value_or("") : "");
    return std::move(query).Build(request.limit + 1);
}

// Варианты одного фильтра: остальные условия сохраняются, собственное исключается.
inline registry::BuiltQuery BuildRegistryValues(const models::ListContractsRequest& request,
    const std::string& organization, const std::string& field,
    const std::optional<std::string>& search, const std::optional<std::string>& after, int limit,
    const std::optional<std::vector<std::string>>& candidates = std::nullopt) {
    const auto& definition = registry::FindField(registry_fields::kFields, field);
    auto builder = BuildRegistryFilters(request, organization, field);
    registry::ApplyCandidates(builder, definition, candidates);
    return registry::BuildValues(
        std::move(builder), sql::kRegistryFilterValues, definition, search, after, limit + 1);
}
}  // namespace contracts::helpers
