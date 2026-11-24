-- Нулевая редакция маршрута означает, что пока получены только данные клиента.
INSERT INTO shipment_projections (organization_id, shipment_id, revision, customer_revision, customer_id)
VALUES ($1::UUID, $2::UUID, 0, $3::BIGINT, $4::UUID)
ON CONFLICT (organization_id, shipment_id) DO UPDATE
SET customer_revision = EXCLUDED.customer_revision, customer_id = EXCLUDED.customer_id
WHERE shipment_projections.customer_revision IS NULL
    OR shipment_projections.customer_revision < EXCLUDED.customer_revision
RETURNING customer_revision;
