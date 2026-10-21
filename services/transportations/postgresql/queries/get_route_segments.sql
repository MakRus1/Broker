SELECT document
FROM route_segments
WHERE organization_id = $1::UUID AND transportation_id = $2::UUID
ORDER BY position;
