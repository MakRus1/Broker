-- $1 — организация.
-- $2 — идентификатор роли.
-- $3 — идентификатор реестра.
-- $4 — настройки реестра по умолчанию в JSON.
-- $5 — ожидаемая редакция (0 для создания).
INSERT INTO role_registry_defaults (organization_id, role, registry, value)
SELECT $1::UUID, $2, $3, $4::JSONB
WHERE $5::BIGINT = 0
ON CONFLICT DO NOTHING
RETURNING role, registry, value, revision;
