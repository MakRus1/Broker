-- $1 — хеш одноразового состояния входа.
-- $2 — секрет PKCE.
-- $3 — nonce для проверки ID-токена.
-- $4 — срок действия попытки входа.
-- $5 — максимальное число попыток входа.
INSERT INTO browser_login_attempts (id, verifier, nonce, expires_at)
SELECT $1, $2, $3, $4
WHERE (SELECT count(*) FROM browser_login_attempts) < $5
RETURNING id;
