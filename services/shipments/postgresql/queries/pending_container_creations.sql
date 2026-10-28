SELECT ROW(operation.id, item.container_type_code, item.container_ownership,
    operation.quantity::BIGINT)::public.pending_container_creation_v1
FROM creation_operations AS operation
JOIN shipment_items AS item
    ON item.organization_id = operation.organization_id AND item.id = operation.item_id
WHERE item.organization_id = $1::UUID AND item.shipment_id = $2::UUID
    AND operation.status = 'pending';
