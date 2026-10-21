INSERT INTO shipments (organization_id, customer_id, reference)
VALUES ($1::UUID, $2::UUID, $3)
RETURNING ROW(
    id,
    revision,
    'S-' || number,
    customer_id,
    reference
)::public.shipment_v1;
