SELECT ROW(
    id,
    actor,
    action,
    to_char(occurred_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS.US"Z"'),
    document
)::public.transportation_event_v1
FROM business_audit
WHERE organization_id = $1::UUID
  AND entity_id = $2::UUID
  AND action IN ('set_route', 'record_actual', 'correct_actual')
  AND ($3::BIGINT IS NULL OR id < $3::BIGINT)
ORDER BY id DESC
LIMIT $4;
