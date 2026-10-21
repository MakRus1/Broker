SELECT ROW(m.subject, m.username, m.display_name, m.email, m.role, m.enabled, m.revision)::public.member_v1
FROM memberships m
WHERE m.organization_id = $1::UUID AND m.issuer = $2 AND m.subject = $3;
