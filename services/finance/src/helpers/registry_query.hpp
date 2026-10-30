#pragma once
#include <helpers/registry.hpp>
#include <platform/registry_page.hpp>

namespace finance::helpers {
namespace registry = platform::registry;

namespace registry_fields {

// SQL-поля и подписи каталога финансовых меток.
inline constexpr registry::Field<std::string> kId{"id", "id", "UUID", true};
inline constexpr registry::Field<std::string> kOrganization{
    "organization_id", "organization_id", "UUID"};
inline constexpr registry::Field<std::string> kName{"name", "lower(name) COLLATE \"C\""};
inline constexpr std::string_view kDirectionLabel =
    R"(CASE direction WHEN 'income' THEN 'Доход' WHEN 'expense' THEN 'Расход' ELSE 'Доход и расход' END COLLATE "C")";
inline constexpr std::string_view kUnitLabel =
    R"(CASE unit WHEN 'container' THEN 'Контейнер' WHEN 'transportation' THEN 'Перевозка' WHEN 'ton' THEN 'Тонна' WHEN 'day' THEN 'Сутки' ELSE 'Штука' END COLLATE "C")";
inline constexpr auto kDirection =
    registry::Field<std::string>{"direction", "direction"}.SortedAs(kDirectionLabel);
inline constexpr auto kUnit = registry::Field<std::string>{"unit", "unit"}.SortedAs(kUnitLabel);
inline constexpr registry::Field<std::string> kCurrency{"currency", "currency"};
inline constexpr registry::Field<std::string> kVatMode{"vat_mode", "vat_mode"};
inline constexpr auto kArchived = registry::Field<bool>{"archived", "archived", "BOOLEAN"}.SortedAs(
    "(CASE WHEN archived THEN '1' ELSE '0' END) COLLATE \"C\"");
inline constexpr std::array<registry::RegistryField, 7> kFields{{
    {.Field = kId, .Label = "Идентификатор", .ValueSql = "id", .Visible = false},
    {.Field = kName, .Label = "Наименование", .ValueSql = "name", .FoldValues = true},
    {.Field = kDirection, .Label = "Направление", .ValueSql = kDirectionLabel},
    {.Field = kUnit, .Label = "Единица расчёта", .ValueSql = kUnitLabel},
    {.Field = kCurrency, .Label = "Валюта", .ValueSql = "currency"},
    {.Field = kVatMode,
        .Label = "НДС",
        .ValueSql = "CASE vat_mode WHEN 'exempt' THEN 'Без НДС' ELSE 'Ставка НДС' END"},
    {.Field = kArchived, .Label = "Состояние", .ValueSql = "archived"},
}};
inline constexpr auto kSortableFields = registry::SortableFields(kFields);
}  // namespace registry_fields

// Общие условия страницы и списка вариантов: организация и применённые фильтры.
inline registry::QueryBuilder BuildRegistryFilters(
    const models::ListFinancialLabelsRequest& request, const std::string& organization,
    const std::optional<std::string>& exclude = std::nullopt) {
    using namespace registry_fields;
    registry::QueryBuilder query{sql::kListFinancialLabels,
        registry::RegistryOrderBy(kFields, request.sort), request.descending};
    query.WhereEqual(kOrganization, organization);
    registry::ApplyFilters(query, kFields, request.filters, exclude);
    return query;
}

inline const registry::Definition kRegistry{
    registry_fields::kFields, sql::kRegistryFilterValues, &BuildRegistryFilters};

}  // namespace finance::helpers
