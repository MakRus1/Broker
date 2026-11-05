SELECT payload = $5::JSONB
FROM notification_events
WHERE producer = $1
    AND organization_id = $2::UUID
    AND shipment_id = $3::UUID
    AND revision = $4;
