-- $1 — хеш идентификатора сессии.
DELETE FROM browser_sessions
WHERE id = $1 AND expires_at > now()
RETURNING refresh_token;
