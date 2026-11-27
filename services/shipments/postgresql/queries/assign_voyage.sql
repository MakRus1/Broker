-- $1 — организация, $2 — поставка, $3 — участок, $4 — рейс,
-- $5 и $6 — позиции заходов для погрузки и выгрузки.
INSERT INTO voyage_assignments (organization_id, shipment_id, segment_id, voyage_id, loading_call, discharge_call)
SELECT s.organization_id, s.id, leg.id, loading.voyage_id, loading.position, discharge.position
FROM shipments s
CROSS JOIN LATERAL unnest(s.route_segments) leg
JOIN line_operations.voyage_port_calls loading ON loading.organization_id = $1
    AND loading.voyage_id = $4 AND loading.position = $5
JOIN line_operations.voyage_port_calls discharge ON discharge.organization_id = loading.organization_id
    AND discharge.voyage_id = loading.voyage_id
WHERE s.organization_id = $1 AND s.id = $2 AND leg.id = $3 AND leg.type = 'sea'
    AND ((leg.sea).booking).status = 'confirmed' AND discharge.position = $6
ON CONFLICT (organization_id, shipment_id, segment_id) DO UPDATE
SET voyage_id = EXCLUDED.voyage_id, loading_call = EXCLUDED.loading_call, discharge_call = EXCLUDED.discharge_call
RETURNING segment_id;
