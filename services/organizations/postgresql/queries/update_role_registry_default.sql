-- $1 — организация.
-- $2 — идентификатор роли.
-- $3 — идентификатор реестра.
-- $4 — настройки реестра по умолчанию в JSON.
-- $5 — ожидаемая редакция.
UPDATE role_registry_defaults
SET value = $4::JSONB, revision = revision + 1
WHERE organization_id = $1::UUID AND role = $2 AND registry = $3 AND revision = $5::BIGINT
RETURNING role, registry, value, revision;
