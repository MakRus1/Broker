INSERT INTO memberships AS m (
    organization_id, issuer, subject, username, display_name, email, role, last_name, first_name, middle_name
)
SELECT $1::UUID, $2, v.subject, v.username, v.display_name, v.email, v.role, v.last_name, v.first_name, v.middle_name
FROM (SELECT ($3::public.member_v2).*) AS v
RETURNING ROW(m.subject, m.username, m.display_name, m.email, m.role, m.enabled, m.revision, m.last_name, m.first_name, m.middle_name)::public.member_v2;
