WITH shipment_transportations AS (
    SELECT type, container_type_code, container_ownership, operation_id
    FROM transportations
    WHERE organization_id = $1::UUID AND shipment_id = $2::UUID
), totals AS (
    SELECT container_type_code, container_ownership, NULL::UUID AS operation_id,
        COUNT(*) AS quantity
    FROM shipment_transportations
    WHERE type = 'container'
    GROUP BY container_type_code, container_ownership

    UNION ALL

    SELECT NULL::TEXT, NULL::TEXT, operation_id, COUNT(*)
    FROM shipment_transportations
    WHERE operation_id = ANY($3::UUID[])
    GROUP BY operation_id
)
SELECT ROW(container_type_code, container_ownership, operation_id, quantity)::public.container_fact_v1
FROM totals
ORDER BY container_type_code NULLS LAST, container_ownership NULLS LAST, operation_id;
