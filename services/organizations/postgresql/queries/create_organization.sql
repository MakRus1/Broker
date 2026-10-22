INSERT INTO organizations AS o (id, name)
VALUES ($1::UUID, $2)
RETURNING ROW(o.id, o.name, o.enabled, o.revision)::public.organization_v1;
