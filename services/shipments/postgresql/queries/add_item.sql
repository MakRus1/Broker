INSERT INTO shipment_items (
    organization_id, shipment_id, container_type_code, container_ownership, planned_quantity
)
VALUES ($1::UUID, $2::UUID, $3, $4, $5)
ON CONFLICT (organization_id, shipment_id, container_type_code, container_ownership)
    WHERE NOT archived
DO UPDATE SET planned_quantity = shipment_items.planned_quantity + EXCLUDED.planned_quantity,
    revision = shipment_items.revision + 1
RETURNING ROW(
    id,
    revision,
    shipment_id,
    container_type_code,
    container_ownership,
    planned_quantity::BIGINT
)::public.shipment_item_v1;
