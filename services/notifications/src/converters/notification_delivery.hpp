#pragma once
#include <models/notification_delivery.hpp>
#include <openapi/models.hpp>

namespace notifications::converters {
inline NotificationDelivery Convert(const models::ShipmentNotificationPayload& payload) {
    NotificationDelivery delivery{.RecipientIssuer = payload.recipient_issuer,
        .RecipientSubject = payload.recipient_subject,
        .EntityId = payload.shipment_id,
        .EntityNumber = payload.shipment_number,
        .Revision = payload.revision};
    if (delivery.RecipientSubject == payload.actor_subject)
        delivery.RecipientSubject.reset();
    return delivery;
}

inline NotificationDelivery Convert(const models::InquiryShipmentCreationFailedPayload& payload) {
    NotificationDelivery delivery{.RecipientIssuer = payload.recipient_issuer,
        .RecipientSubject = payload.recipient_subject,
        .EntityId = payload.inquiry_id,
        .EntityNumber = payload.inquiry_number,
        .Revision = payload.revision};
    return delivery;
}

inline NotificationDelivery Convert(const models::RfqNotificationPayload& payload) {
    NotificationDelivery delivery{.RecipientIssuer = payload.recipient_issuer,
        .RecipientSubject = payload.recipient_subject,
        .EntityId = payload.rfq_id,
        .EntityNumber = payload.rfq_title,
        .Revision = payload.revision};
    if (delivery.RecipientSubject == payload.actor_subject)
        delivery.RecipientSubject.reset();
    return delivery;
}
}  // namespace notifications::converters
