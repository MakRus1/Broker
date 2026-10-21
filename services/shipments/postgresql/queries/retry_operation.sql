UPDATE creation_operations
SET
    next_attempt_at = now() + make_interval(secs => LEAST(60, attempts * 2)),
    lease_until = NULL,
    lease_token = NULL
WHERE organization_id = $1::UUID
  AND id = $2::UUID
  AND lease_token = $3::UUID
  AND status = 'pending';
