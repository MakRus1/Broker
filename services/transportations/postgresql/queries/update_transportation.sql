UPDATE transportations
SET type = $3,
    hazardous = $4,
    container_type_code = $5,
    container_ownership = $6,
    revision = revision + 1
WHERE organization_id = $1::UUID AND id = $2::UUID
RETURNING ROW(id, revision, shipment_id, item_id, operation_id, container_type_code,
    container_ownership, 'T-' || number, type, hazardous)::public.transportation_v3;
