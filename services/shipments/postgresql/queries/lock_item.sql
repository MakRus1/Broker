SELECT ROW(
    id,
    revision,
    shipment_id,
    container_type_code,
    container_ownership,
    planned_quantity::BIGINT
)::public.shipment_item_v1
FROM shipment_items
WHERE organization_id = $1::UUID AND id = $2::UUID AND NOT archived
FOR UPDATE;
