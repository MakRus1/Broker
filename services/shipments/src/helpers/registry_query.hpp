#pragma once
#include <helpers/registry.hpp>
#include <platform/registry_query.hpp>

namespace shipments::helpers {
namespace registry = platform::registry;

namespace registry_fields {
using Date = userver::utils::datetime::Date;
inline constexpr registry::Field<std::string> kId{"id", "id", "UUID", true};
inline constexpr registry::Field<std::string> kOrganization{
    "organization_id", "organization_id", "UUID"};
inline constexpr registry::Field<std::string> kNumber{"number", "lower(number) COLLATE \"C\""};
inline constexpr registry::Field<std::string> kCargo{
    "cargo_description", "lower(COALESCE(cargo_description, '')) COLLATE \"C\""};
inline constexpr registry::Field<std::string> kCustomer{"customer_id", "customer_id", "UUID"};
inline constexpr auto kStatus =
    registry::Field<std::string>{"status", "status"}.SortedAs("lower(status) COLLATE \"C\"");
inline constexpr registry::Field<Date> kDate{"shipment_date", "shipment_date", "DATE"};
inline constexpr std::array<registry::FieldDefinition, 5> kSortableFields{
    kId, kNumber, kCargo, kStatus, kDate};
}  // namespace registry_fields

inline registry::BuiltQuery BuildRegistryQuery(
    const models::ListShipmentsRequest& request, const std::string& organization) {
    using namespace registry_fields;
    const auto order = registry::OrderBy(kSortableFields, models::EnumValue(request.sort));
    registry::QueryBuilder query{sql::kListShipments, order, request.descending};
    query.WhereEqual(kOrganization, organization);
    query.WhereContains(kNumber, request.number_query);
    query.WhereContains(kCargo, request.cargo_query);
    query.WhereEqual(kCustomer, request.customer_id);
    query.WhereAny(kStatus, request.statuses);
    query.WhereAtLeast(kDate, request.date_from);
    query.WhereAtMost(kDate, request.date_to);
    const auto after =
        request.cursor.has_value() ? std::optional{request.cursor.value().id} : request.after;
    if (after.has_value() && order.PgType == "DATE")
        query.After(after, request.cursor.value().date_value.value());
    else
        query.After(after,
            request.cursor.has_value() ? request.cursor.value().text_value.value_or("") : "");
    return std::move(query).Build(request.limit + 1);
}
}  // namespace shipments::helpers
