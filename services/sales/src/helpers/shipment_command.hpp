#pragma once
#include <clients/shipments/client.hpp>
#include <platform/outbox/component.hpp>

namespace sales {
using ShipmentCommand = clients::shipments::models::EnsureShipmentRequest;
inline constexpr platform::outbox::Task<ShipmentCommand> kCreateShipment{"shipments.create"};
}  // namespace sales
