#pragma once
#include <map>
#include <openapi/models.hpp>
#include <set>
#include <string>

namespace sales::models {
struct ShipmentRouteReferences {
    std::set<std::string> LocationIds;
    std::set<std::string> CounterpartyIds;
};

using TransportLocations = std::map<std::string, TransportLocation>;
}  // namespace sales::models
