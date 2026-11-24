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
LEFT JOIN shipment_customer_projections AS shipment_customer
    ON shipment_customer.organization_id = transportations.organization_id
    AND shipment_customer.shipment_id = transportations.shipment_id
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
