-- $1 — организация.
-- $2 — идентификатор перевозки.
SELECT ROW(
    id,
    revision,
    shipment_id,
    item_id,
    operation_id,
    container_type_code,
    container_ownership,
    container_number,
    type,
    hazardous,
    route_segments,
    container_iso_code
)::public.transportation_v6
FROM transportations
WHERE organization_id = $1::UUID AND id = $2::UUID
FOR UPDATE;
