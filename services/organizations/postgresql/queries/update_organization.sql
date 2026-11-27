-- $1 — идентификатор организации.
-- $2 — новое название организации.
-- $3 — новый признак активности организации.
-- $4 — новое состояние рабочего места линии (NULL сохраняет текущее).
UPDATE organizations AS o
SET name = $2, enabled = $3, line_operations_enabled = COALESCE($4, o.line_operations_enabled), revision = revision + 1
WHERE id = $1::UUID
RETURNING ROW(o.id, o.name, o.enabled, o.revision, o.line_operations_enabled)::public.organization_v2;
