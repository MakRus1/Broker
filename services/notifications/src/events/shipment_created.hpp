#pragma once
#include <events/shipment_route_changed.hpp>

namespace notifications::events {
inline void Validate(
    const platform::auth::Identity& identity, const models::ShipmentCreated& event) {
    if (event.payload.recipient_issuer != identity.Issuer)
        throw platform::http::HandlerError<400>{"invalid_recipient_issuer",
            "Получатель должен принадлежать провайдеру пользователей отправителя"};
}

inline void Apply(
    userver::storages::postgres::Transaction& transaction, const models::ShipmentCreated& event) {
    const auto& payload = event.payload;
    if (!payload.recipient_subject.has_value() ||
        payload.recipient_subject.value() == payload.actor_subject)
        return;
    transaction.Execute(sql::kCreateShipmentNotification, event.organization_id,
        payload.recipient_issuer, payload.recipient_subject.value(), payload.shipment_id,
        payload.shipment_number, payload.revision);
}
}  // namespace notifications::events
