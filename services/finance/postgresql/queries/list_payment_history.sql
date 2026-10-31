WITH history AS (
SELECT 'payment'::TEXT AS source, event.payment_id AS entity_id,
       event.revision, event.actor, event.changed_at AS occurred_at,
       event.snapshot, previous.snapshot AS previous
FROM payment_history AS event
LEFT JOIN payment_history AS previous
    ON previous.organization_id = event.organization_id
   AND previous.payment_id = event.payment_id
   AND previous.revision = event.revision - 1
WHERE event.organization_id = $1::UUID
  AND event.payment_id = $2::UUID AND $8::BOOLEAN
)
SELECT source, entity_id, revision, actor, occurred_at, snapshot, previous
FROM history
WHERE $3::TIMESTAMPTZ IS NULL
   OR (occurred_at, source, entity_id, revision)
      < ($3::TIMESTAMPTZ, $4::TEXT, $5::UUID, $6::BIGINT)
ORDER BY occurred_at DESC, source DESC, entity_id DESC, revision DESC
LIMIT $7;
