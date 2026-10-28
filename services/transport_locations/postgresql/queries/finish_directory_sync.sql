UPDATE directory_sync
SET lease = NULL,
    lease_until = NULL,
    failure_count = 0,
    next_attempt_at = now() + $6::INTEGER * INTERVAL '1 second',
    updated_at = now(),
    source_hash = $3,
    record_count = $4,
    source_revision = $5
WHERE name = $1 AND lease = $2::UUID;
