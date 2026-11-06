#pragma once
#include <notifications/sql_queries.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/handler_error.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace notifications::events {
inline void Validate(
    const platform::auth::Identity& identity, const models::ShipmentRouteChanged& event) {
    if (event.payload.recipient_issuer != identity.Issuer)
        throw platform::http::HandlerError<400>{"invalid_recipient_issuer",
            "Получатель должен принадлежать провайдеру пользователей отправителя"};
}

inline void Apply(userver::storages::postgres::Transaction& transaction,
    const models::ShipmentRouteChanged& event) {
    const auto& payload = event.payload;
    if (!payload.recipient_subject.has_value() ||
        payload.recipient_subject.value() == payload.actor_subject)
        return;
    transaction.Execute(sql::kCreateNotification, event.organization_id, payload.recipient_issuer,
        payload.recipient_subject.value(), payload.shipment_id, payload.shipment_number,
        payload.revision);
}
}  // namespace notifications::events
