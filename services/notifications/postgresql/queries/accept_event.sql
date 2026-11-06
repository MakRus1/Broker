INSERT INTO notification_events (
    producer, organization_id, event_id, event_type, event_version, payload
)
VALUES ($1, $2::UUID, $3, $4, $5, $6::JSONB)
ON CONFLICT (producer, organization_id, event_id) DO NOTHING
RETURNING event_id;
