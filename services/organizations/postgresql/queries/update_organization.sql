UPDATE organizations AS o
SET name = $2, enabled = $3, revision = revision + 1
WHERE id = $1::UUID
RETURNING ROW(o.id, o.name, o.enabled, o.revision)::public.organization_v1;
