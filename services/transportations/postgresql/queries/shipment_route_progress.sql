-- $1 — организация.
-- $2 — поставка.
SELECT route_segments
FROM transportations
WHERE organization_id = $1::UUID AND shipment_id = $2::UUID;
