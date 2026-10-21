UPDATE shipment_items
SET
    planned_quantity = $3,
    revision = revision + 1
WHERE organization_id = $1::UUID AND id = $2::UUID
RETURNING ROW(
    id,
    revision,
    shipment_id,
    container_type_code,
    container_ownership,
    planned_quantity::BIGINT
)::public.shipment_item_v1;
