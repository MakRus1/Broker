SELECT
    id::TEXT,
    revision,
    'S-' || number AS number,
    customer_id::TEXT,
    reference
FROM shipments
WHERE tenant_id = $1::UUID AND ($2::UUID IS NULL OR id > $2::UUID)
ORDER BY id
LIMIT $3;
