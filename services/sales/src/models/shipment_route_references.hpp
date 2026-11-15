#pragma once
#include <map>
#include <openapi/models.hpp>
#include <set>
#include <string>

namespace sales::models {
struct ShipmentRouteReferences {
    std::set<boost::uuids::uuid> LocationIds;
    std::set<boost::uuids::uuid> CounterpartyIds;
};

using TransportLocations = std::map<boost::uuids::uuid, TransportLocation>;
}  // namespace sales::models
