UPDATE shipments
SET
    reference = $3,
    revision = revision + 1
WHERE tenant_id = $1::UUID AND id = $2::UUID AND revision = $4
RETURNING
    id::TEXT,
    revision,
    'S-' || number AS number,
    customer_id::TEXT,
    reference;
