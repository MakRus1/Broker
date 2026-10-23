INSERT INTO shipment_route_segment_usage (organization_id, shipment_id, segment_id)
SELECT $1::UUID, $2::UUID, unnest($3::UUID[])
ON CONFLICT DO NOTHING;
