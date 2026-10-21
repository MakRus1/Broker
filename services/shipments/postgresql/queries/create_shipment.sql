INSERT INTO shipments (tenant_id, customer_id, reference)
VALUES ($1::UUID, $2::UUID, $3)
RETURNING
    id::TEXT,
    revision,
    'S-' || number AS number,
    customer_id::TEXT,
    reference;
