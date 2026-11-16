-- $1 — хеш идентификатора сессии.
-- $2 — новый токен доступа.
-- $3 — новый токен обновления.
-- $4 — срок действия нового токена доступа.
UPDATE browser_sessions
SET access_token = $2, refresh_token = $3, token_expires_at = $4
WHERE id = $1;
