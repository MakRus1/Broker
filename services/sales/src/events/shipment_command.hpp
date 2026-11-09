#pragma once
#include <clients/notifications/client.hpp>
#include <clients/shipments/client.hpp>
#include <platform/outbox/component.hpp>

namespace sales {
using ShipmentCommand = clients::shipments::models::EnsureShipmentRequest;
inline constexpr platform::outbox::Task<ShipmentCommand> kCreateShipment{"shipments.create"};
inline constexpr platform::outbox::Task<clients::notifications::models::PublishedEvent>
    kPublishNotification{"notifications.publish"};
}  // namespace sales
