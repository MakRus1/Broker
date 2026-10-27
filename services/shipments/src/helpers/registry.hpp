#pragma once
#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/registry.hpp>

namespace shipments::helpers {
struct RegistryRow {
    pgtypes::ShipmentV4 Shipment;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};

inline std::vector<models::Shipment> Convert(const std::vector<RegistryRow>& rows) {
    std::vector<models::Shipment> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(Convert(row.Shipment));
    return result;
}
}  // namespace shipments::helpers
