-- $1 — идентификатор инициатора действия.
-- $2 — issuer токена пользователя.
-- $3 — subject пользователя.
-- $4 — список платформенных прав пользователя.
-- $5 — номер записываемой редакции доступа.
INSERT INTO platform_access_audit(actor, issuer, subject, permissions, revision)
VALUES ($1, $2, $3, $4::TEXT[], $5);
