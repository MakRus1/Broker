UPDATE route_projection_outbox
SET next_attempt_at = now() + INTERVAL '5 seconds', lease_until = NULL, lease_token = NULL
WHERE organization_id = $1::UUID AND shipment_id = $2::UUID AND lease_token = $3::UUID;
