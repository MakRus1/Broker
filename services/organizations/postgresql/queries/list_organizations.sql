SELECT
    ROW(
        o.id,
        o.name,
        o.enabled,
        o.revision
    )::public.organization_v1,
    {sort_key} AS sort_key
FROM organizations AS o
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
