#pragma once
#include <helpers/registry.hpp>
#include <platform/registry_filters.hpp>

namespace shipments::helpers {
namespace registry = platform::registry;

namespace registry_fields {
using Date = userver::utils::datetime::Date;
inline constexpr registry::Field<std::string> kId{"id", "id", "UUID", true};
inline constexpr registry::Field<std::string> kOrganization{
    "organization_id", "organization_id", "UUID"};
inline constexpr auto kNumber =
    registry::Field<std::string>{"number", "lower(number) COLLATE \"C\""}.SortedAs(
        "lower(number) COLLATE public.registry_number_order");
inline constexpr registry::Field<std::string> kCargo{
    "cargo_description", "lower(COALESCE(cargo_description, '')) COLLATE \"C\""};
inline constexpr registry::Field<std::string> kCustomer{"customer_id", "customer_id", "UUID"};
inline constexpr auto kStatus =
    registry::Field<std::string>{"status", "status"}.SortedAs("lower(status) COLLATE \"C\"");
inline constexpr registry::Field<Date> kDate{"shipment_date", "shipment_date", "DATE"};
inline constexpr std::array<registry::RegistryField, 6> kFields{{
    {.Field = kId, .Label = "Идентификатор", .ValueSql = "id", .Visible = false},
    {.Field = kNumber, .Label = "Номер поставки", .ValueSql = "number", .FoldValues = true},
    {.Field = kCargo,
        .Label = "Груз",
        .ValueSql = "COALESCE(cargo_description, '')",
        .FoldValues = true},
    {.Field = kCustomer,
        .Label = "Клиент",
        .ValueSql = "customer_id",
        .Sortable = false,
        .Lookup = "counterparties"},
    {.Field = kStatus, .Label = "Статус", .ValueSql = "status"},
    {.Field = kDate, .Label = "Дата поставки", .ValueSql = "shipment_date"},
}};
inline constexpr auto kSortableFields = registry::SortableFields(kFields);
}  // namespace registry_fields

inline registry::QueryBuilder BuildRegistryFilters(const models::ListShipmentsRequest& request,
    const std::string& organization, const std::optional<std::string>& exclude = std::nullopt) {
    using namespace registry_fields;
    const auto order = registry::RegistryOrderBy(kFields, request.sort);
    registry::QueryBuilder query{sql::kListShipments, order, request.descending};
    query.WhereEqual(kOrganization, organization);
    registry::ApplyFilters(query, kFields, request.filters, exclude);
    return query;
}

inline registry::BuiltQuery BuildRegistryQuery(
    const models::ListShipmentsRequest& request, const std::string& organization) {
    using namespace registry_fields;
    auto query = BuildRegistryFilters(request, organization);
    const auto order = registry::RegistryOrderBy(kFields, request.sort);
    const auto after =
        request.cursor.has_value() ? std::optional{request.cursor.value().id} : request.after;
    if (after.has_value() && order.CursorType == registry::CursorKeyType::Date)
        query.After(after, request.cursor.value().date_value.value());
    else
        query.After(after,
            request.cursor.has_value() ? request.cursor.value().text_value.value_or("") : "");
    return std::move(query).Build(request.limit + 1);
}

inline registry::BuiltQuery BuildRegistryValues(const models::ListShipmentsRequest& request,
    const std::string& organization, const std::string& field,
    const std::optional<std::string>& search, const std::optional<std::string>& after, int limit,
    const std::optional<std::vector<std::string>>& candidates = std::nullopt) {
    const auto& definition = registry::FindField(registry_fields::kFields, field);
    auto builder = BuildRegistryFilters(request, organization, field);
    registry::ApplyCandidates(builder, definition, candidates);
    return registry::BuildValues(
        std::move(builder), sql::kRegistryFilterValues, definition, search, after, limit + 1);
}
}  // namespace shipments::helpers
