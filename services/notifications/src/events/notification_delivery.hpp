#pragma once
#include <notifications/sql_queries.hpp>
#include <openapi/models.hpp>
#include <optional>
#include <platform/auth/identity.hpp>
#include <platform/handler_error.hpp>
#include <string>
#include <userver/storages/postgres/transaction.hpp>

namespace notifications::events {
struct NotificationDelivery {
    std::string RecipientIssuer;
    std::optional<std::string> RecipientSubject;
    std::string EntityId;
    std::string EntityNumber;
    std::int64_t Revision;

    explicit NotificationDelivery(const models::ShipmentNotificationPayload& payload)
        : RecipientIssuer(payload.recipient_issuer), RecipientSubject(payload.recipient_subject),
          EntityId(payload.shipment_id), EntityNumber(payload.shipment_number),
          Revision(payload.revision) {
        if (RecipientSubject == payload.actor_subject)
            RecipientSubject.reset();
    }

    explicit NotificationDelivery(const models::InquiryShipmentCreationFailedPayload& payload)
        : RecipientIssuer(payload.recipient_issuer), RecipientSubject(payload.recipient_subject),
          EntityId(payload.inquiry_id), EntityNumber(payload.inquiry_number),
          Revision(payload.revision) {}

    explicit NotificationDelivery(const models::RfqNotificationPayload& payload)
        : RecipientIssuer(payload.recipient_issuer), RecipientSubject(payload.recipient_subject),
          EntityId(payload.rfq_id), EntityNumber(payload.rfq_title), Revision(payload.revision) {
        if (RecipientSubject == payload.actor_subject)
            RecipientSubject.reset();
    }
};

inline void ValidateRecipient(
    const platform::auth::Identity& identity, const NotificationDelivery& delivery) {
    if (delivery.RecipientIssuer != identity.Issuer)
        throw platform::http::HandlerError<400>{"invalid_recipient_issuer",
            "Получатель должен принадлежать провайдеру пользователей отправителя"};
}

inline void ApplyNotification(userver::storages::postgres::Transaction& transaction,
    const std::string& organization, const NotificationDelivery& delivery,
    const std::string& event_type) {
    if (!delivery.RecipientSubject.has_value())
        return;
    transaction.Execute(sql::kCreateNotification, organization, delivery.RecipientIssuer,
        delivery.RecipientSubject.value(), delivery.EntityId, delivery.EntityNumber,
        delivery.Revision, event_type);
}
}  // namespace notifications::events
