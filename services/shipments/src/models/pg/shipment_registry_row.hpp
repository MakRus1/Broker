#pragma once
#include <models/pg/shipment_raw.hpp>
#include <optional>
#include <string>

namespace shipments::models::pg {
struct ShipmentRegistryRow {
    ShipmentRaw Shipment;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};
}  // namespace shipments::models::pg
