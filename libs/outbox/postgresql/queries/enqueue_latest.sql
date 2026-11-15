-- $1 — тема сообщения.
-- $2 — ключ сообщения в пределах темы.
-- $3 — данные сообщения в JSON.
INSERT INTO outbox (topic, message_key, payload)
VALUES ($1::TEXT, $2::TEXT, $3::JSONB)
ON CONFLICT (topic, message_key) DO UPDATE
SET payload = EXCLUDED.payload,
    generation = outbox.generation + 1,
    attempts = 0,
    available_at = now(),
    lease_token = NULL;
