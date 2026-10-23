SELECT segment_id::TEXT
FROM shipment_route_segment_usage
WHERE organization_id = $1::UUID AND shipment_id = $2::UUID;
