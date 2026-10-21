UPDATE shipment_items
SET
    planned_quantity = $3,
    revision = revision + 1
WHERE tenant_id = $1::UUID AND id = $2::UUID
RETURNING
    id::TEXT,
    revision,
    shipment_id::TEXT,
    container_type_code,
    container_ownership,
    planned_quantity::BIGINT;
