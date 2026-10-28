UPDATE airport_sync
SET lease = $1::UUID, next_attempt_at = now() + INTERVAL '5 minutes'
WHERE name = 'ourairports' AND next_attempt_at <= now()
RETURNING name;
