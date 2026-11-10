#pragma once
#include <map>
#include <models/container_totals.hpp>
#include <openapi/models.hpp>

namespace shipments {
using ContainerKey = std::pair<std::string, std::string>;

struct ContainerPlan {
    std::map<ContainerKey, ContainerTotals> Totals;
    std::vector<models::ContainerFact> Unplanned;
};

}  // namespace shipments
