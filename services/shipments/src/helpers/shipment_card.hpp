#pragma once
#include <configs/keys.hpp>
#include <models/pg/shipment_data.hpp>
#include <models/pg/shipment_dictionary_value.hpp>
#include <models/pg/shipment_raw.hpp>
#include <openapi/models.hpp>

namespace shipments::helpers {
models::pg::ShipmentData CreateShipmentCard(
    const models::CreateShipmentRequest& request, const configs::ShipmentSettings& settings);
models::pg::ShipmentData UpdateShipmentCard(
    const models::UpdateShipmentRequest& request, const models::pg::ShipmentRaw& previous);
void ValidateCard(const models::pg::ShipmentData& card, const configs::ShipmentSettings& settings,
    const std::vector<models::pg::ShipmentDictionaryValue>& dictionaries,
    const models::pg::ShipmentRaw* previous = nullptr);
std::vector<std::string> ChangedRouteSegments(
    const std::vector<models::ShipmentRouteSegment>& previous,
    const std::vector<models::ShipmentRouteSegment>& next);
}  // namespace shipments::helpers
