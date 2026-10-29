WITH candidate AS (
    SELECT id
    FROM outbox
    WHERE topic = $1::TEXT
      AND available_at <= now()
    ORDER BY available_at, id
    FOR UPDATE SKIP LOCKED
    LIMIT 1
)
UPDATE outbox AS message
SET available_at = now() + make_interval(secs => $2::DOUBLE PRECISION),
    lease_token = gen_random_uuid(),
    attempts = message.attempts + 1
FROM candidate
WHERE message.id = candidate.id
RETURNING message.id::TEXT, message.topic, message.payload,
          message.lease_token::TEXT, message.generation;
