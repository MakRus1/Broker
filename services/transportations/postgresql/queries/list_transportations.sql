SELECT
    ROW(
        id,
        revision,
        shipment_id,
        item_id,
        operation_id,
        container_type_code,
        container_ownership,
        container_number,
        type,
        hazardous
    )::public.transportation_v4 AS transportation,
    id,
    organization_id,
    shipment_id,
    item_id,
    container_number,
    container_type_code,
    type,
    hazardous
FROM transportations
