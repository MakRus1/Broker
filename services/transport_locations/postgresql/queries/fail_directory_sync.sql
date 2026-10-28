UPDATE directory_sync
SET lease = NULL,
    lease_until = NULL,
    next_attempt_at = last_attempt_at + LEAST($4::INTEGER,
        $3::INTEGER * power(2, LEAST(failure_count - 1, 20))) * INTERVAL '1 second'
WHERE name = $1 AND lease = $2::UUID;
