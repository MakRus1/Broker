UPDATE shipments
SET
    reference = $3,
    revision = revision + 1
WHERE organization_id = $1::UUID AND id = $2::UUID AND revision = $4
RETURNING ROW(
    id,
    revision,
    'S-' || number,
    customer_id,
    reference
)::public.shipment_v1;
