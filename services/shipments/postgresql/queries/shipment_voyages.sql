-- $1 — организация, $2 — идентификатор поставки.
SELECT b.discharge_port, b.load_port, b.segment_id, v.id AS voyage_id, v.number AS voyage_number
FROM confirmed_voyage_bookings b
JOIN line_operations.voyages v ON (v.organization_id, v.id) = (b.organization_id, b.voyage_id)
WHERE b.organization_id = $1 AND b.shipment_id = $2 ORDER BY b.segment_id;
