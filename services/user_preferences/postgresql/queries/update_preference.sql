-- $1 — организация.
-- $2 — issuer токена пользователя.
-- $3 — subject пользователя.
-- $4 — ключ настройки.
-- $5 — значение настройки в JSON.
-- $6 — ожидаемая редакция настройки.
UPDATE user_preferences
SET value = $5::JSONB,
    revision = revision + 1
WHERE organization_id = $1::UUID
    AND issuer = $2::TEXT
    AND subject = $3::TEXT
    AND key = $4::TEXT
    AND revision = $6::BIGINT
RETURNING key, value, revision;
