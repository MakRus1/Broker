SELECT COALESCE(record_count, 0)
FROM airport_sync
WHERE name = 'ourairports' AND lease = $1::UUID AND next_attempt_at > now()
FOR UPDATE;
