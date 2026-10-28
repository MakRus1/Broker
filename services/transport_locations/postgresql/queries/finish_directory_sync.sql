UPDATE directory_sync
SET lease = NULL,
    next_attempt_at = now() + INTERVAL '24 hours',
    updated_at = now(),
    source_hash = $3,
    record_count = $4,
    source_revision = $5
WHERE name = $1 AND lease = $2::UUID;
