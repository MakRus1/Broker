UPDATE outbox
SET available_at = now() + make_interval(
        secs => LEAST($4::BIGINT, attempts::BIGINT * $3::BIGINT)::DOUBLE PRECISION),
    lease_token = NULL
WHERE id = $1::UUID
  AND lease_token = $2::UUID;
