SELECT
    ROW(
        o.id,
        o.name,
        o.enabled,
        o.revision,
        o.line_operations_enabled
    )::public.organization_v2,
    {sort_key} AS sort_key
FROM organizations AS o
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
