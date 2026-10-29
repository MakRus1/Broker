DELETE FROM route_projection_outbox
WHERE organization_id = $1::UUID AND shipment_id = $2::UUID AND lease_token = $3::UUID;
