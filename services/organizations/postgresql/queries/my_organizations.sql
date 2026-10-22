SELECT ROW(o.id, o.name, o.enabled, o.revision)::public.organization_v1
FROM organizations o
JOIN memberships m ON m.organization_id = o.id
WHERE m.issuer = $1 AND m.subject = $2 AND m.enabled AND o.enabled
ORDER BY o.id;
