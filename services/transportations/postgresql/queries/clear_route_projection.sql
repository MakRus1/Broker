-- $1 — организация.
-- $2 — идентификатор поставки.
DELETE FROM shipment_route_segments
WHERE organization_id = $1::UUID AND shipment_id = $2::UUID;
