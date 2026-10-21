SELECT ROW(
    id,
    revision,
    shipment_id,
    item_id,
    operation_id,
    container_type_code,
    container_ownership
)::public.transportation_v1
FROM transportations
WHERE tenant_id = $1::UUID AND id = $2::UUID;
