UPDATE airport_sync
SET lease = NULL,
    next_attempt_at = now() + INTERVAL '24 hours',
    updated_at = now(),
    source_hash = $2,
    record_count = $3
WHERE name = 'ourairports' AND lease = $1::UUID;
