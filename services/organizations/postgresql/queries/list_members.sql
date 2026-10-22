SELECT ROW(m.subject, m.username, m.display_name, m.email, m.role, m.enabled, m.revision, m.last_name, m.first_name, m.middle_name)::public.member_v2
FROM memberships m
WHERE m.organization_id = $1::UUID AND ($2::TEXT IS NULL OR m.subject > $2)
ORDER BY m.subject
LIMIT $3;
