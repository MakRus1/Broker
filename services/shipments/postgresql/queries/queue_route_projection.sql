INSERT INTO route_projection_outbox (organization_id, shipment_id, revision, route_segments)
VALUES ($1::UUID, $2::UUID, $3::BIGINT, $4::JSONB)
ON CONFLICT (organization_id, shipment_id) DO UPDATE
SET revision = EXCLUDED.revision,
    route_segments = EXCLUDED.route_segments,
    next_attempt_at = now(),
    lease_until = NULL,
    lease_token = NULL;
