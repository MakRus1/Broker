#pragma once
#include <helpers/registry.hpp>
#include <helpers/registry_query_builder.hpp>
#include <stdexcept>

namespace transportations::helpers {
namespace registry_fields {
inline constexpr registry::Field<std::string> kOrganization{"organization_id", "UUID"};
inline constexpr registry::Field<std::string> kNumber{
    "lower(COALESCE(container_number, '')) COLLATE \"C\"", "TEXT"};
inline constexpr registry::Field<std::string> kType{"type", "TEXT"};
inline constexpr registry::Field<std::string> kContainerType{"container_type_code", "TEXT"};
inline constexpr registry::Field<bool> kHazardous{"hazardous", "BOOLEAN"};
inline constexpr registry::Field<std::string> kShipment{"shipment_id", "UUID"};
inline constexpr registry::Field<std::string> kItem{"item_id", "UUID"};
}  // namespace registry_fields

inline registry::Order RegistryOrder(models::ListTransportationsRequest_sort sort) {
    using Sort = models::ListTransportationsRequest_sort;
    switch (sort) {
    case Sort::ContainerNumber:
        return {registry_fields::kNumber.Expression};
    case Sort::ContainerType:
        return {"COALESCE(container_type_code, '') COLLATE \"C\""};
    case Sort::Type:
        return {"type COLLATE \"C\""};
    case Sort::Id:
        return {"'' COLLATE \"C\"", true};
    }
    throw std::logic_error{"Неизвестная сортировка перевозок"};
}

inline registry::BuiltQuery BuildRegistryQuery(
    const models::ListTransportationsRequest& request, const std::string& organization) {
    using namespace registry_fields;
    registry::QueryBuilder query{
        sql::kListTransportations, RegistryOrder(request.sort), request.descending};
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
