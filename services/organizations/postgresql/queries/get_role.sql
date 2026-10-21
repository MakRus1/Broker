SELECT ROW(r.id, r.name, r.description, r.permissions, r.revision)::public.organization_role_v1
FROM organization_roles r
WHERE r.organization_id = $1::UUID AND r.id = $2;
