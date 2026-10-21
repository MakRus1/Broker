SELECT ROW(m.subject, m.username, m.display_name, m.email, m.role, m.enabled, m.revision)::public.member_v1
FROM memberships m
WHERE m.organization_id = $1::UUID AND ($2::TEXT IS NULL OR m.subject > $2)
ORDER BY m.subject
LIMIT $3;
