#pragma once
#include <map>
#include <models/container_totals.hpp>
#include <models/pg/pending_container_creation.hpp>
#include <models/pg/shipment_item.hpp>
#include <openapi/models.hpp>

namespace shipments {
using ContainerKey = std::pair<std::string, std::string>;

struct ContainerPlanSnapshot {
    std::vector<models::pg::ShipmentItem> Items;
    std::vector<models::pg::PendingContainerCreation> Pending;
};

struct ContainerPlan {
    std::map<ContainerKey, ContainerTotals> Totals;
    std::vector<models::ContainerFact> Unplanned;
};

}  // namespace shipments
