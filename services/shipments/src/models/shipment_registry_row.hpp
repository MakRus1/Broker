#pragma once
#include <models/pg/shipment_raw.hpp>
#include <optional>
#include <string>

namespace shipments::models {
struct ShipmentRegistryRow {
    pg::ShipmentRaw Shipment;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};
}  // namespace shipments::models
