SELECT ROW(
    id,
    revision,
    shipment_id,
    item_id,
    operation_id,
    container_type_code,
    container_ownership,
    'T-' || number
)::public.transportation_v2
FROM transportations
WHERE tenant_id = $1::UUID AND id = $2::UUID
FOR UPDATE;
