-- $1 — хеш одноразового состояния входа.
DELETE FROM browser_login_attempts
WHERE id = $1 AND expires_at > now()
RETURNING verifier, nonce;
