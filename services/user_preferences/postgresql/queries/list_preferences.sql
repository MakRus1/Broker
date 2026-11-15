-- $1 — организация.
-- $2 — issuer токена пользователя.
-- $3 — subject пользователя.
SELECT key, value, revision
FROM user_preferences
WHERE organization_id = $1::UUID
    AND issuer = $2::TEXT
    AND subject = $3::TEXT
ORDER BY key;
