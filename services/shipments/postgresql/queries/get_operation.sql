SELECT
    id::TEXT,
    item_id::TEXT,
    quantity::BIGINT,
    status,
    result
FROM creation_operations
WHERE tenant_id = $1::UUID AND id = $2::UUID;
