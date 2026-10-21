SELECT ROW(
    COALESCE(SUM(quantity) FILTER (WHERE status = 'succeeded'), 0)::BIGINT,
    COALESCE(SUM(quantity) FILTER (WHERE status = 'pending'), 0)::BIGINT
)::public.quantity_totals_v1
FROM creation_operations
WHERE organization_id = $1::UUID AND item_id = $2::UUID;
