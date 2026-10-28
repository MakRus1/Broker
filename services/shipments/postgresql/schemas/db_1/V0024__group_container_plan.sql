-- Старые строки остаются для истории и неизменяемых payload операций.
ALTER TABLE shipment_items ADD COLUMN archived BOOLEAN NOT NULL DEFAULT FALSE;
ALTER TABLE shipment_items DROP CONSTRAINT shipment_items_planned_quantity_check;
ALTER TABLE shipment_items ADD CONSTRAINT shipment_items_planned_quantity_check
    CHECK (planned_quantity >= 0);

WITH grouped AS (
    SELECT organization_id, shipment_id, container_type_code, container_ownership,
        (array_agg(id ORDER BY id))[1] AS canonical_id,
        SUM(planned_quantity)::INTEGER AS total,
        COUNT(*) AS row_count
    FROM shipment_items
    GROUP BY organization_id, shipment_id, container_type_code, container_ownership
)
UPDATE shipment_items AS item
SET planned_quantity = CASE WHEN item.id = grouped.canonical_id THEN grouped.total
        ELSE item.planned_quantity END,
    archived = item.id <> grouped.canonical_id,
    revision = item.revision + CASE
        WHEN item.id = grouped.canonical_id AND grouped.row_count > 1 THEN 1 ELSE 0 END
FROM grouped
WHERE item.organization_id = grouped.organization_id AND item.shipment_id = grouped.shipment_id
    AND item.container_type_code = grouped.container_type_code
    AND item.container_ownership = grouped.container_ownership;

CREATE UNIQUE INDEX shipment_items_active_container_idx
    ON shipment_items (organization_id, shipment_id, container_type_code, container_ownership)
    WHERE NOT archived;
CREATE TYPE pending_container_creation_v1 AS (
    id UUID,
    container_type TEXT,
    ownership TEXT,
    quantity BIGINT
);

-- Результат команды содержит план. Актуальный факт читается отдельно.
UPDATE idempotent_results
SET result = result - ARRAY['created_quantity', 'pending_quantity', 'available_quantity', 'excess_quantity']
WHERE action = 'add_item';
DROP TYPE item_totals_v1;
DROP TYPE quantity_totals_v1;
