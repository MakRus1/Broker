-- $1 — идентификатор организации.
-- $2 — название организации.
-- $3 — включить рабочее место морской линии.
INSERT INTO organizations AS o (id, name, line_operations_enabled)
VALUES ($1::UUID, $2, $3)
RETURNING ROW(o.id, o.name, o.enabled, o.revision, o.line_operations_enabled)::public.organization_v2;
