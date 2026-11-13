#pragma once
#include <boost/uuid/uuid_io.hpp>
#include <models/notification_row.hpp>
#include <openapi/models.hpp>

namespace notifications::converters {

inline models::Notification Convert(const models::NotificationRow& row) {
    return {.created_at = row.CreatedAt.GetUnderlying(),
        .entity_id = boost::uuids::to_string(row.EntityId),
        .entity_number = row.EntityNumber,
        .entity_revision = row.EntityRevision,
        .event_type = models::ParseEnumScalar(row.EventType, models::Notification_event_type{}),
        .id = boost::uuids::to_string(row.Id),
        .read = row.Read};
}
}  // namespace notifications::converters
