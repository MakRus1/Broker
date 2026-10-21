UPDATE memberships AS m
SET role = $4, enabled = $5, revision = revision + 1
WHERE organization_id = $1::UUID AND issuer = $2 AND subject = $3
RETURNING ROW(m.subject, m.username, m.display_name, m.email, m.role, m.enabled, m.revision)::public.member_v1;
