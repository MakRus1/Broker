-- $1 — организация, $2 — изменённая поставка.
DELETE FROM voyage_assignments a WHERE a.organization_id = $1 AND a.shipment_id = $2
AND NOT EXISTS (SELECT 1 FROM confirmed_voyage_bookings b
    WHERE (b.organization_id, b.shipment_id, b.segment_id) = (a.organization_id, a.shipment_id, a.segment_id));
