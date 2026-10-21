SELECT ROW(o.id, o.name, o.enabled, o.revision)::public.organization_v1
FROM organizations o
WHERE o.id = $1::UUID
FOR UPDATE;
