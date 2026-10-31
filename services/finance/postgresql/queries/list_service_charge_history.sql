WITH related_documents AS MATERIALIZED (
    -- GIN ищет только документы, когда-либо связанные с этой карточкой.
    SELECT DISTINCT invoice_id
    FROM invoice_history
    WHERE organization_id = $1::UUID AND $8::BOOLEAN
      AND snapshot @> jsonb_build_object('charges',
          jsonb_build_array(jsonb_build_object('id', $2::TEXT)))
), related_revisions AS (
SELECT 'invoice'::TEXT AS source, event.invoice_id AS entity_id,
       event.revision, event.actor, event.occurred_at AS occurred_at,
       event.snapshot, previous.snapshot AS previous
FROM invoice_history AS event
LEFT JOIN invoice_history AS previous
    ON previous.organization_id = event.organization_id
   AND previous.invoice_id = event.invoice_id
   AND previous.revision = event.revision - 1
WHERE event.organization_id = $1::UUID
  AND event.invoice_id IN (SELECT invoice_id FROM related_documents)
), history AS (
SELECT 'service_charge'::TEXT AS source, event.service_charge_id AS entity_id,
       event.revision, event.actor, event.occurred_at AS occurred_at,
       event.snapshot, previous.snapshot AS previous
FROM service_charge_history AS event
LEFT JOIN service_charge_history AS previous
    ON previous.organization_id = event.organization_id
   AND previous.service_charge_id = event.service_charge_id
   AND previous.revision = event.revision - 1
WHERE event.organization_id = $1::UUID
  AND event.service_charge_id = $2::UUID
UNION ALL
SELECT source, entity_id, revision, actor, occurred_at, snapshot, previous
FROM related_revisions
WHERE (previous IS NOT NULL OR revision = 1)
  AND (
      COALESCE(snapshot->'charges', '[]'::JSONB)
          @> jsonb_build_array(jsonb_build_object('id', $2::TEXT))
      AND COALESCE(snapshot#>>'{invoice,archived}', 'false') <> 'true'
  ) IS DISTINCT FROM (
      COALESCE(previous->'charges', '[]'::JSONB)
          @> jsonb_build_array(jsonb_build_object('id', $2::TEXT))
      AND COALESCE(previous#>>'{invoice,archived}', 'false') <> 'true'
  )
)
SELECT source, entity_id, revision, actor, occurred_at, snapshot, previous
FROM history
WHERE $3::TIMESTAMPTZ IS NULL
   OR (occurred_at, source, entity_id, revision)
      < ($3::TIMESTAMPTZ, $4::TEXT, $5::UUID, $6::BIGINT)
ORDER BY occurred_at DESC, source DESC, entity_id DESC, revision DESC
LIMIT $7;
