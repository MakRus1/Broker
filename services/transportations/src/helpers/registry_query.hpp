#pragma once
#include <helpers/registry.hpp>
#include <helpers/registry_query_builder.hpp>

namespace transportations::helpers {
namespace registry_fields {
inline constexpr registry::Field<std::string> kId{"id", "id", "UUID", true};
inline constexpr registry::Field<std::string> kOrganization{
    "organization_id", "organization_id", "UUID"};
inline constexpr registry::Field<std::string> kNumber{
    "container_number", "lower(COALESCE(container_number, '')) COLLATE \"C\""};
inline constexpr registry::Field<std::string> kType{"type", "type COLLATE \"C\""};
inline constexpr registry::Field<std::string> kContainerType{
    "container_type", "COALESCE(container_type_code, '') COLLATE \"C\""};
inline constexpr registry::Field<bool> kHazardous{"hazardous", "hazardous", "BOOLEAN"};
inline constexpr registry::Field<std::string> kShipment{"shipment_id", "shipment_id", "UUID"};
inline constexpr registry::Field<std::string> kItem{"item_id", "item_id", "UUID"};
inline constexpr std::array kSortableFields{kId, kNumber, kContainerType, kType};
}  // namespace registry_fields

inline registry::BuiltQuery BuildRegistryQuery(
    const models::ListTransportationsRequest& request, const std::string& organization) {
    using namespace registry_fields;
    registry::QueryBuilder query{sql::kListTransportations,
        registry::OrderBy(kSortableFields, models::EnumValue(request.sort)), request.descending};
    query.WhereEqual(kOrganization, organization);
    query.WhereContains(kNumber, request.query);
    query.WhereAny(kType, Convert(request.types));
    query.WhereAny(kContainerType, request.container_types);
    query.WhereEqual(kHazardous, request.hazardous);
    query.WhereEqual(kShipment, request.shipment_id);
    query.WhereEqual(kItem, request.item_id);
    query.After(
        request.cursor.has_value() ? std::optional{request.cursor.value().id} : request.after,
        request.cursor.has_value() ? request.cursor.value().value : "");
    return std::move(query).Build(request.limit + 1);
}
}  // namespace transportations::helpers
