-- $1 — организация, $2 — идентификатор рейса.
SELECT COALESCE(booking_number, '') AS booking_number, discharge_port, load_port, shipment_id, shipment_number
FROM confirmed_voyage_bookings WHERE organization_id = $1 AND voyage_id = $2
ORDER BY shipment_number, segment_id;
