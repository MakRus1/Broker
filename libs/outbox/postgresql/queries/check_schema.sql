SELECT id, topic, message_key, payload, generation, attempts, created_at, available_at, lease_token
FROM outbox
LIMIT 0;
