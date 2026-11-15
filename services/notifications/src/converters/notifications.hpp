#pragma once
#include <models/notification_row.hpp>
#include <openapi/models.hpp>

namespace notifications::converters {

inline models::Notification Convert(const models::NotificationRow& row) {
    return {.created_at = row.CreatedAt.GetUnderlying(),
        .entity_id = row.EntityId,
        .entity_number = row.EntityNumber,
        .entity_revision = row.EntityRevision,
        .event_type = models::ParseEnumScalar(row.EventType, models::Notification_event_type{}),
        .id = row.Id,
        .read = row.Read};
}
}  // namespace notifications::converters
