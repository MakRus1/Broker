SELECT
    id::TEXT,
    revision,
    shipment_id::TEXT,
    container_type_code,
    container_ownership,
    planned_quantity::BIGINT
FROM shipment_items
WHERE tenant_id = $1::UUID AND shipment_id = $2::UUID
  AND ($3::UUID IS NULL OR id > $3::UUID)
ORDER BY id
LIMIT $4;
