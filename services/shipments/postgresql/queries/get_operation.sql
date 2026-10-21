SELECT ROW(
    id,
    item_id,
    quantity::BIGINT,
    status,
    result
)::public.creation_operation_v1
FROM creation_operations
WHERE organization_id = $1::UUID AND id = $2::UUID;
