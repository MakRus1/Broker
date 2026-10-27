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
    {sort_key} AS sort_key
FROM transportations
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
