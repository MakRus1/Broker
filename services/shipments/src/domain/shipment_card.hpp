#pragma once
#include <configs/keys.hpp>
#include <models/pg/shipment_data.hpp>
#include <models/pg/shipment_dictionary_value.hpp>
#include <models/pg/shipment_raw.hpp>
#include <openapi/models.hpp>

namespace shipments::domain {
std::optional<std::string> CleanText(const std::optional<std::string>& value);
models::pg::ShipmentData CreateShipmentCard(const models::CreateShipmentRequest& request,
    const configs::detail_SHIPMENT_SETTINGS::Value& settings);
models::pg::ShipmentData UpdateShipmentCard(
    const models::UpdateShipmentRequest& request, const models::pg::ShipmentRaw& previous);
void ValidateCard(const models::pg::ShipmentData& card,
    const configs::detail_SHIPMENT_SETTINGS::Value& settings,
    const std::vector<models::pg::ShipmentDictionaryValue>& dictionaries,
    const models::pg::ShipmentRaw* previous = nullptr);
std::vector<std::string> ChangedRouteSegments(
    const models::pg::ShipmentRaw& previous, const std::vector<models::ShipmentRouteSegment>& next);
void ValidateRouteUsage(const models::pg::ShipmentRaw& previous,
    const models::pg::ShipmentData& next, const std::vector<std::string>& protected_ids);
}  // namespace shipments::domain
