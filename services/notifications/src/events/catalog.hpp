#pragma once
#include <events/registry.hpp>
#include <events/shipment_route_changed.hpp>

namespace notifications::events {
inline const Registry& Catalog() {
    static const Registry registry = [] {
        Registry result;
        result.Register<models::ShipmentRouteChanged>("shipment.route_changed", 1,
            "shipments-dispatcher", "notifications.publish_shipments", ValidateShipmentRouteChanged,
            HandleShipmentRouteChanged);
        return result;
    }();
    return registry;
}
}  // namespace notifications::events
