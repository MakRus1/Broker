UPDATE directory_sync
SET lease = $2::UUID, next_attempt_at = now() + INTERVAL '5 minutes'
WHERE name = $1 AND next_attempt_at <= now()
RETURNING name;
