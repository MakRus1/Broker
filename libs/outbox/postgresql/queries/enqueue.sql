INSERT INTO outbox (topic, message_key, payload)
VALUES ($1::TEXT, $2::TEXT, $3::JSONB)
ON CONFLICT (topic, message_key) DO NOTHING;
