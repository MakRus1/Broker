-- $1 — организация, $2 — поставка, $3 — участок.
DELETE FROM voyage_assignments WHERE organization_id = $1 AND shipment_id = $2 AND segment_id = $3;
