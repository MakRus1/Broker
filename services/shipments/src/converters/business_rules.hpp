#pragma once
#include <openapi/models.hpp>
#include <postgresql/types.hpp>

namespace shipments::converters {
inline pgtypes::ShipmentBusinessRulesV1 Convert(const models::ShipmentBusinessRules& value) {
    return {.NumberTemplate = value.number_template,
        .ResetNumberYearly = value.reset_number_yearly,
        .Statuses = value.statuses,
        .RouteTypes = value.route_types,
        .PlannedContainers = value.planned_containers,
        .ContainerTypes = value.container_types};
}

inline models::ShipmentBusinessRules Convert(const pgtypes::ShipmentBusinessRulesV1& value) {
    return {.container_types = value.ContainerTypes,
        .number_template = value.NumberTemplate,
        .planned_containers = value.PlannedContainers,
        .reset_number_yearly = value.ResetNumberYearly,
        .route_types = value.RouteTypes,
        .statuses = value.Statuses};
}

}  // namespace shipments::converters
