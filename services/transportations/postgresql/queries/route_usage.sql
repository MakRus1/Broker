-- $1 — организация.
-- $2 — идентификатор поставки.
SELECT segment.document
FROM transportations AS transportation
CROSS JOIN LATERAL jsonb_array_elements(transportation.route_segments) AS segment(document)
WHERE transportation.organization_id = $1::UUID
  AND transportation.shipment_id = $2::UUID;
