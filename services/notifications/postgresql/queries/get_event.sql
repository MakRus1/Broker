SELECT event_type = $4 AND event_version = $5 AND payload = $6::JSONB
FROM notification_events
WHERE producer = $1
    AND organization_id = $2::UUID
    AND event_id = $3;
