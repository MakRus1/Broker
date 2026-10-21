SELECT
    id::TEXT,
    revision,
    shipment_id::TEXT,
    item_id::TEXT,
    operation_id::TEXT,
    container_type_code,
    container_ownership
FROM transportations
WHERE tenant_id = $1::UUID AND id = $2::UUID
FOR UPDATE;
