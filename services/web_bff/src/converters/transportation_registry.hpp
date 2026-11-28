#pragma once
#include <clients/counterparties/client.hpp>
#include <clients/shipments/client.hpp>
#include <clients/transportations/client.hpp>
#include <openapi/models.hpp>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

// Номер поставки и клиент для строк реестра перевозок.
namespace web_bff::converters {
inline std::set<boost::uuids::uuid> ShipmentIds(
    const std::vector<clients::transportations::models::Transportation>& transportations) {
    std::set<boost::uuids::uuid> result;
    for (const auto& transportation : transportations)
        result.insert(transportation.shipment_id);
    return result;
}

inline std::set<boost::uuids::uuid> CustomerIds(
    const std::vector<clients::shipments::models::Shipment>& shipments) {
    std::set<boost::uuids::uuid> result;
    for (const auto& shipment : shipments)
        result.insert(shipment.customer_id);
    return result;
}

// Без поставок поле не возвращается, без клиента у ссылки нет customer_name.
inline std::optional<std::vector<models::transportations::TransportationShipmentReference>>
ToShipmentReferences(const std::vector<clients::shipments::models::Shipment>& shipments,
    const std::vector<clients::counterparties::models::Counterparty>& customers) {
    if (shipments.empty())
        return std::nullopt;
    std::map<boost::uuids::uuid, std::string> customer_names;
    for (const auto& customer : customers)
        customer_names.emplace(customer.id, customer.name);
    std::vector<models::transportations::TransportationShipmentReference> result;
    result.reserve(shipments.size());
    for (const auto& shipment : shipments) {
        auto& reference = result.emplace_back();
        reference.id = shipment.id;
        reference.number = shipment.number;
        if (const auto name = customer_names.find(shipment.customer_id);
            name != customer_names.end())
            reference.customer_name = name->second;
    }
    return result;
}
}  // namespace web_bff::converters
