SELECT ROW(
    event.id, event.actor, event.action,
    event.occurred_at,
    event.document, previous.result
)::public.counterparty_event_v2
FROM business_audit AS event
LEFT JOIN LATERAL (
    -- Предыдущее событие может находиться за границей запрошенной страницы.
    SELECT document->'result' AS result
    FROM business_audit
    WHERE organization_id = event.organization_id
      AND entity_id = event.entity_id
      AND id < event.id
      AND action IN ('create_counterparty', 'update_counterparty',
                     'archive_counterparty', 'restore_counterparty')
    ORDER BY id DESC
    LIMIT 1
) AS previous ON TRUE
WHERE event.organization_id = $1::UUID
  AND event.entity_id = $2::UUID
  AND event.action IN ('create_counterparty', 'update_counterparty',
                       'archive_counterparty', 'restore_counterparty')
  AND ($3::BIGINT IS NULL OR event.id < $3::BIGINT)
ORDER BY event.id DESC
LIMIT $4;
