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
    const clients::notifications::models::ShipmentNotificationPayload payload{
        .actor_subject = actor.Subject,
        .recipient_issuer = actor.Issuer,
        .recipient_subject = after.logist.value_or(std::nullopt),
        .revision = after.revision,
        .shipment_id = after.id,
        .shipment_number = after.number};
    const auto id = organization + ":" + after.id + ":" + std::to_string(after.revision);
    const clients::notifications::models::PublishedEvent event{
        .value = clients::notifications::models::ShipmentRouteChanged{
            .id = id, .organization_id = organization, .payload = payload}};
    platform::outbox::Enqueue(
        transaction, kPublishNotificationEvent, id, json::ValueBuilder(event).ExtractValue());
}

inline void QueueShipmentCreated(pg::Transaction& transaction, const std::string& organization,
    const platform::auth::Identity& actor, const models::Shipment& shipment) {
    const auto id = organization + ":created:" + shipment.id;
    const clients::notifications::models::PublishedEvent event{
        .value = clients::notifications::models::ShipmentCreated{.id = id,
            .organization_id = organization,
            .payload = {.actor_subject = actor.Subject,
                .recipient_issuer = actor.Issuer,
                .recipient_subject = shipment.logist.value_or(std::nullopt),
                .revision = shipment.revision,
                .shipment_id = shipment.id,
                .shipment_number = shipment.number}}};
    platform::outbox::Enqueue(transaction, kPublishNotificationEvent, id,
        userver::formats::json::ValueBuilder(event).ExtractValue());
}
}  // namespace shipments::helpers
