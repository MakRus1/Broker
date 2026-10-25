SELECT ROW(
    id,
    actor,
    action,
    occurred_at,
    document
)::public.transportation_event_v2
FROM business_audit
WHERE organization_id = $1::UUID
  AND entity_id = $2::UUID
  AND action IN ('set_route', 'record_actual', 'correct_actual')
  AND ($3::BIGINT IS NULL OR id < $3::BIGINT)
ORDER BY id DESC
LIMIT $4;
