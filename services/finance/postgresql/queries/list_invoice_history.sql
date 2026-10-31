WITH related_documents AS MATERIALIZED (
    -- GIN ищет только документы, когда-либо связанные с этой карточкой.
    SELECT DISTINCT payment_id
    FROM payment_history
    WHERE organization_id = $1::UUID AND $8::BOOLEAN
      AND snapshot @> jsonb_build_object('allocations',
          jsonb_build_array(jsonb_build_object('invoice_id', $2::TEXT)))
), related_revisions AS (
SELECT 'payment'::TEXT AS source, event.payment_id AS entity_id,
       event.revision, event.actor, event.changed_at AS occurred_at,
       event.snapshot, previous.snapshot AS previous
FROM payment_history AS event
LEFT JOIN payment_history AS previous
    ON previous.organization_id = event.organization_id
   AND previous.payment_id = event.payment_id
   AND previous.revision = event.revision - 1
WHERE event.organization_id = $1::UUID
  AND event.payment_id IN (SELECT payment_id FROM related_documents)
), related_allocations AS (
    SELECT source, entity_id, revision, actor, occurred_at, snapshot, previous,
        CASE WHEN snapshot#>>'{payment,archived}' = 'true' THEN 0
        ELSE COALESCE((
            SELECT (allocation->>'amount_minor')::BIGINT
            FROM jsonb_array_elements(snapshot->'allocations') AS allocation
            WHERE allocation->>'invoice_id' = $2::TEXT
        ), 0) END AS allocated_minor,
        CASE WHEN previous#>>'{payment,archived}' = 'true' THEN 0
        ELSE COALESCE((
            SELECT (allocation->>'amount_minor')::BIGINT
            FROM jsonb_array_elements(previous->'allocations') AS allocation
            WHERE allocation->>'invoice_id' = $2::TEXT
        ), 0) END AS previous_allocated_minor
    FROM related_revisions
), history AS (
SELECT 'invoice'::TEXT AS source, event.invoice_id AS entity_id,
       event.revision, event.actor, event.occurred_at AS occurred_at,
       event.snapshot, previous.snapshot AS previous
FROM invoice_history AS event
LEFT JOIN invoice_history AS previous
    ON previous.organization_id = event.organization_id
   AND previous.invoice_id = event.invoice_id
   AND previous.revision = event.revision - 1
WHERE event.organization_id = $1::UUID
  AND event.invoice_id = $2::UUID
UNION ALL
SELECT source, entity_id, revision, actor, occurred_at, snapshot, previous
FROM related_allocations
WHERE (previous IS NOT NULL OR revision = 1)
  AND allocated_minor IS DISTINCT FROM previous_allocated_minor
)
SELECT source, entity_id, revision, actor, occurred_at, snapshot, previous
FROM history
WHERE $3::TIMESTAMPTZ IS NULL
   OR (occurred_at, source, entity_id, revision)
      < ($3::TIMESTAMPTZ, $4::TEXT, $5::UUID, $6::BIGINT)
ORDER BY occurred_at DESC, source DESC, entity_id DESC, revision DESC
LIMIT $7;
