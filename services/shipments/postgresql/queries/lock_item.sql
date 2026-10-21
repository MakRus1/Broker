SELECT
    id::TEXT,
    revision,
    shipment_id::TEXT,
    container_type_code,
    container_ownership,
    planned_quantity::BIGINT
FROM shipment_items
WHERE tenant_id = $1::UUID AND id = $2::UUID
FOR UPDATE;
