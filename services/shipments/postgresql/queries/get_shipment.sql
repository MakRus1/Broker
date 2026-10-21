SELECT ROW(
    id,
    revision,
    'S-' || number,
    customer_id,
    reference
)::public.shipment_v1
FROM shipments
WHERE tenant_id = $1::UUID AND id = $2::UUID;
