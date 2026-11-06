#pragma once
#include <clients/notifications/client.hpp>
#include <helpers/database.hpp>
#include <helpers/outbox.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/outbox/component.hpp>
#include <userver/formats/json.hpp>

namespace shipments::helpers {
inline void QueueRouteChanged(pg::Transaction& transaction, const std::string& organization,
    const platform::auth::Identity& actor, const models::Shipment& before,
    const models::Shipment& after) {
    namespace json = userver::formats::json;
    if (json::ValueBuilder(before.route_segments).ExtractValue() ==
        json::ValueBuilder(after.route_segments).ExtractValue())
        return;
    const clients::notifications::models::ShipmentRouteChanged event{.actor_subject = actor.Subject,
        .organization_id = organization,
        .recipient_issuer = actor.Issuer,
        .recipient_subject = after.logist.value_or(std::nullopt),
        .revision = after.revision,
        .shipment_id = after.id,
        .shipment_number = after.number};
    const auto id = organization + ":" + after.id + ":" + std::to_string(after.revision);
    const clients::notifications::models::PublishedEvent envelope{.id = id,
        .organization_id = organization,
        .payload = json::ValueBuilder(event).ExtractValue(),
        .type = "shipment.route_changed",
        .version = 1};
    platform::outbox::Enqueue(
        transaction, kShipmentRouteChanged, id, json::ValueBuilder(envelope).ExtractValue());
}
}  // namespace shipments::helpers
