-- $1 — хеш идентификатора сессии.
-- $2 — идентификатор учётной записи.
-- $3 — токен доступа.
-- $4 — токен обновления.
-- $5 — CSRF-токен.
-- $6 — срок действия токена доступа.
-- $7 — срок действия сессии.
INSERT INTO browser_sessions (id, actor, access_token, refresh_token, csrf, token_expires_at, expires_at)
SELECT $1, $2, $3, $4, $5, $6, $7
WHERE (SELECT count(*) FROM browser_sessions) < 2000
RETURNING id;
