#pragma once
#include <notifications/sql_queries.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/handler_error.hpp>
#include <string>
#include <userver/storages/postgres/transaction.hpp>

namespace notifications::events {
inline void ValidateShipmentRecipient(
    const platform::auth::Identity& identity, const models::ShipmentNotificationPayload& payload) {
    if (payload.recipient_issuer != identity.Issuer)
        throw platform::http::HandlerError<400>{"invalid_recipient_issuer",
            "Получатель должен принадлежать провайдеру пользователей отправителя"};
}

inline void ApplyShipmentNotification(userver::storages::postgres::Transaction& transaction,
    const std::string& organization, const models::ShipmentNotificationPayload& payload,
    const std::string& event_type) {
    if (!payload.recipient_subject.has_value() ||
        payload.recipient_subject.value() == payload.actor_subject)
        return;
    transaction.Execute(sql::kCreateNotification, organization, payload.recipient_issuer,
        payload.recipient_subject.value(), payload.shipment_id, payload.shipment_number,
        payload.revision, event_type);
}
}  // namespace notifications::events
