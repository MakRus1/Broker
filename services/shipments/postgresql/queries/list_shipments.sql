SELECT ROW(
    id,
    revision,
    'S-' || number,
    customer_id,
    reference
)::public.shipment_v1
FROM shipments
WHERE tenant_id = $1::UUID AND ($2::UUID IS NULL OR id > $2::UUID)
ORDER BY id
LIMIT $3;
