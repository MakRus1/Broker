SELECT ROW(
    id,
    revision,
    shipment_id,
    item_id,
    operation_id,
    container_type_code,
    container_ownership,
    container_number,
    type,
    hazardous
)::public.transportation_v4
FROM transportations
WHERE organization_id = $1::UUID AND id = $2::UUID
FOR UPDATE;
