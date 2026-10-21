SELECT
    id::TEXT,
    revision,
    'S-' || number AS number,
    customer_id::TEXT,
    reference
FROM shipments
WHERE tenant_id = $1::UUID AND id = $2::UUID;
