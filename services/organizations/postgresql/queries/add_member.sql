INSERT INTO memberships AS m (
    organization_id, issuer, subject, username, display_name, email, role
)
SELECT $1::UUID, $2, v.subject, v.username, v.display_name, v.email, v.role
FROM (SELECT ($3::public.member_v1).*) AS v
RETURNING ROW(m.subject, m.username, m.display_name, m.email, m.role, m.enabled, m.revision)::public.member_v1;
