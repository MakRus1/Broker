INSERT INTO notification_events (producer, organization_id, shipment_id, revision, payload)
VALUES ($1, $2::UUID, $3::UUID, $4, $5::JSONB)
ON CONFLICT (producer, organization_id, shipment_id, revision) DO NOTHING
RETURNING revision;
