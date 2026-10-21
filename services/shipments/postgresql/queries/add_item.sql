INSERT INTO shipment_items (
    tenant_id, shipment_id, container_type_code, container_ownership, planned_quantity
)
VALUES ($1::UUID, $2::UUID, $3, $4, $5)
RETURNING
    id::TEXT,
    revision,
    shipment_id::TEXT,
    container_type_code,
    container_ownership,
    planned_quantity::BIGINT;
