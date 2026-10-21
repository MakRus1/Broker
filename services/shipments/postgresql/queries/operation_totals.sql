SELECT
    COALESCE(SUM(quantity) FILTER (WHERE status = 'succeeded'), 0)::BIGINT AS created,
    COALESCE(SUM(quantity) FILTER (WHERE status = 'pending'), 0)::BIGINT AS pending
FROM creation_operations
WHERE tenant_id = $1::UUID AND item_id = $2::UUID;
