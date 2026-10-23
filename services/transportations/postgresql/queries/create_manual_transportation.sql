INSERT INTO transportations (
    organization_id, id, shipment_id, ordinal, type, hazardous, container_type_code, container_ownership
)
VALUES ($1::UUID, $2::UUID, $3::UUID, 1, $4, $5, $6, $7)
RETURNING ROW(id, revision, shipment_id, item_id, operation_id, container_type_code,
    container_ownership, 'T-' || number, type, hazardous)::public.transportation_v3;
