#pragma once
#include <openapi/models.hpp>
#include <userver/storages/postgres/io/chrono.hpp>

namespace notifications::converters {
struct NotificationRow {
    std::string Id;
    std::string EntityId;
    std::string EntityNumber;
    std::int64_t EntityRevision;
    userver::storages::postgres::TimePointTz CreatedAt;
    bool Read;
    std::string EventType;
};

inline models::Notification Convert(const NotificationRow& row) {
    return {.created_at = row.CreatedAt.GetUnderlying(),
        .entity_id = row.EntityId,
        .entity_number = row.EntityNumber,
        .entity_revision = row.EntityRevision,
        .event_type = models::ParseEnumScalar(row.EventType, models::Notification_event_type{}),
        .id = row.Id,
        .read = row.Read};
}
}  // namespace notifications::converters
