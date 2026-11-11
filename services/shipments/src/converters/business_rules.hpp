#pragma once
#include <models/pg/shipment_business_rules.hpp>
#include <openapi/models.hpp>

namespace shipments::converters {
inline models::pg::ShipmentBusinessRules Convert(const models::ShipmentBusinessRules& value) {
    return {.NumberTemplate = value.number_template,
        .ResetNumberYearly = value.reset_number_yearly,
        .Statuses = value.statuses,
        .RouteTypes = value.route_types,
        .PlannedContainers = value.planned_containers,
        .ContainerTypes = value.container_types};
}

inline models::ShipmentBusinessRules Convert(const models::pg::ShipmentBusinessRules& value) {
    return {.container_types = value.ContainerTypes,
        .number_template = value.NumberTemplate,
        .planned_containers = value.PlannedContainers,
        .reset_number_yearly = value.ResetNumberYearly,
        .route_types = value.RouteTypes,
        .statuses = value.Statuses};
}

}  // namespace shipments::converters
