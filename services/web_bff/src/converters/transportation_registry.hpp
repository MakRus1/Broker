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

using Shipments = std::optional<std::vector<clients::shipments::models::Shipment>>;
using Customers = std::optional<std::vector<clients::counterparties::models::Counterparty>>;

inline std::set<boost::uuids::uuid> CustomerIds(const Shipments& shipments) {
    std::set<boost::uuids::uuid> result;
    for (const auto& shipment : shipments.value_or(Shipments::value_type{}))
        result.insert(shipment.customer_id);
    return result;
}

// Без поставок поле не возвращается, без клиента у ссылки нет customer_name.
inline std::optional<std::vector<models::transportations::TransportationShipmentReference>>
ToShipmentReferences(const Shipments& shipments, const Customers& customers) {
    if (!shipments.has_value() || shipments->empty())
        return std::nullopt;
    std::map<boost::uuids::uuid, std::string> customer_names;
    for (const auto& customer : customers.value_or(Customers::value_type{}))
        customer_names.emplace(customer.id, customer.name);
    std::vector<models::transportations::TransportationShipmentReference> result;
    result.reserve(shipments->size());
    for (const auto& shipment : *shipments) {
        auto& reference = result.emplace_back();
        reference.id = shipment.id;
        reference.number = shipment.number;
        if (const auto name = customer_names.find(shipment.customer_id);
            name != customer_names.end())
            reference.customer_name = name->second;
    }
    return result;
}

// Что не загрузилось из-за сбоя соседа. Пустой список — значит, всё на месте.
inline std::optional<std::vector<models::transportations::RegistryReferenceKind>>
UnavailableReferences(const Shipments& shipments, const Customers& customers) {
    using Kind = models::transportations::RegistryReferenceKind;
    std::vector<Kind> result;
    if (!shipments.has_value())
        result.push_back(Kind::Shipments);
    if (!customers.has_value())
        result.push_back(Kind::Customers);
    if (result.empty())
        return std::nullopt;
    return result;
}
}  // namespace web_bff::converters
