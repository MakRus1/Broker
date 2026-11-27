-- $1 — идентификатор организации.
SELECT ROW(o.id, o.name, o.enabled, o.revision, o.line_operations_enabled)::public.organization_v2
FROM organizations o
WHERE o.id = $1::UUID
FOR UPDATE;
