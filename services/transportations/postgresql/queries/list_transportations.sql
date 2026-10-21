SELECT
    id::TEXT,
    revision,
    shipment_id::TEXT,
    item_id::TEXT,
    operation_id::TEXT,
    container_type_code,
    container_ownership
FROM transportations
WHERE tenant_id = $1::UUID
  AND ($2::UUID IS NULL OR id > $2::UUID)
  AND ($3::UUID IS NULL OR shipment_id = $3::UUID)
  AND ($4::UUID IS NULL OR item_id = $4::UUID)
ORDER BY id
LIMIT $5;
