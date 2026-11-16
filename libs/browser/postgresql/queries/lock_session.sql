-- $1 — хеш идентификатора сессии.
SELECT actor, access_token, refresh_token, csrf, token_expires_at, expires_at
FROM browser_sessions
WHERE id = $1 AND expires_at > now()
FOR UPDATE;
