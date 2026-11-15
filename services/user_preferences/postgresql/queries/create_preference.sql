-- $1 — организация.
-- $2 — issuer токена пользователя.
-- $3 — subject пользователя.
-- $4 — ключ настройки.
-- $5 — значение настройки в JSON.
INSERT INTO user_preferences (
    organization_id, issuer, subject, key, value, revision
)
VALUES ($1::UUID, $2::TEXT, $3::TEXT, $4::TEXT, $5::JSONB, 1)
ON CONFLICT (organization_id, issuer, subject, key) DO NOTHING
RETURNING key, value, revision;
