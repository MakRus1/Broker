SELECT segments.document
FROM route_segments AS segments
JOIN transportations AS transportation
  ON transportation.organization_id = segments.organization_id
 AND transportation.id = segments.transportation_id
WHERE transportation.organization_id = $1::UUID
  AND transportation.shipment_id = $2::UUID;
