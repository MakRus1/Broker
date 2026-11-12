#pragma once
#include <converters/shipments.hpp>
#include <models/pg/shipment_registry_row.hpp>

namespace shipments::converters {

inline std::vector<models::Shipment> Convert(
    const std::vector<models::pg::ShipmentRegistryRow>& rows) {
    std::vector<models::Shipment> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(Convert(row.Shipment));
    return result;
}
}  // namespace shipments::converters
