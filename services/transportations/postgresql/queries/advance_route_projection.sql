INSERT INTO shipment_route_projections (organization_id, shipment_id, revision)
VALUES ($1::UUID, $2::UUID, $3::BIGINT)
ON CONFLICT (organization_id, shipment_id) DO UPDATE
SET revision = EXCLUDED.revision
WHERE shipment_route_projections.revision < EXCLUDED.revision
RETURNING revision;
