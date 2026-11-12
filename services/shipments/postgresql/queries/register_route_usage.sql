WITH registered AS (
    INSERT INTO shipment_route_segment_usage (organization_id, shipment_id, segment_id)
    SELECT $1::UUID, $2::UUID, unnest($3::UUID[])
    ON CONFLICT DO NOTHING
)
UPDATE shipments
SET route_usage_version = route_usage_version + 1
WHERE organization_id = $1::UUID AND id = $2::UUID;
