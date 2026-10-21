INSERT INTO organization_roles AS r (organization_id, id, name, description, permissions)
SELECT $1::UUID, v.id, v.name, v.description, v.permissions
FROM (SELECT ($2::public.organization_role_v1).*) v
RETURNING ROW(r.id, r.name, r.description, r.permissions, r.revision)::public.organization_role_v1;
