SELECT
    ROW(
        transportations.id,
        transportations.revision,
        transportations.shipment_id,
        item_id,
        operation_id,
        container_type_code,
        container_ownership,
        container_number,
        type,
        hazardous,
        route_segments
    )::public.transportation_v5 AS transportation,
    {sort_key} AS sort_key
FROM transportations
LEFT JOIN shipment_projections AS shipment_projection
    ON shipment_projection.organization_id = transportations.organization_id
    AND shipment_projection.shipment_id = transportations.shipment_id
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
