#pragma once
#include <events/registry.hpp>

namespace notifications::events {
inline void ValidateShipmentRouteChanged(const platform::auth::Identity& identity,
    const models::PublishedEvent& envelope, const models::ShipmentRouteChanged& event) {
    if (event.organization_id != envelope.organization_id)
        throw ApiError<400>{
            "event_organization_mismatch", "Организация события не совпадает с конвертом"};
    if (event.recipient_issuer != identity.Issuer)
        throw ApiError<400>{"invalid_recipient_issuer",
            "Получатель должен принадлежать провайдеру пользователей отправителя"};
}

inline void HandleShipmentRouteChanged(
    pg::Transaction& transaction, const models::ShipmentRouteChanged& event) {
    if (!event.recipient_subject.has_value() ||
        event.recipient_subject.value() == event.actor_subject)
        return;
    transaction.Execute(sql::kCreateNotification, event.organization_id, event.recipient_issuer,
        event.recipient_subject.value(), event.shipment_id, event.shipment_number, event.revision);
}
}  // namespace notifications::events
