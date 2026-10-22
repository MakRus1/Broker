SELECT ROW(o.id, o.name, o.enabled, o.revision)::public.organization_v1
FROM organizations o
WHERE ($1::UUID IS NULL OR o.id > $1::UUID)
ORDER BY o.id
LIMIT $2;
