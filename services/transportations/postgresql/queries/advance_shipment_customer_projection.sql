INSERT INTO shipment_customer_projections (organization_id, shipment_id, revision, customer_id)
VALUES ($1::UUID, $2::UUID, $3::BIGINT, $4::UUID)
ON CONFLICT (organization_id, shipment_id) DO UPDATE
SET revision = EXCLUDED.revision, customer_id = EXCLUDED.customer_id
WHERE shipment_customer_projections.revision < EXCLUDED.revision
RETURNING revision;
