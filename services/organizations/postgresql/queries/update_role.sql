UPDATE organization_roles AS r
SET name = v.name, description = v.description, permissions = v.permissions, revision = r.revision + 1
FROM (SELECT ($2::public.organization_role_v1).*) v
WHERE r.organization_id = $1::UUID AND r.id = v.id
RETURNING ROW(r.id, r.name, r.description, r.permissions, r.revision)::public.organization_role_v1;
