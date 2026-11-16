-- $1 — организация.
-- $2 — issuer токена пользователя.
-- $3 — subject пользователя.
SELECT ROW(m.subject, m.username, p.display_name, m.email, m.role, m.enabled, m.revision, p.last_name, p.first_name, p.middle_name)::public.member_v2
FROM memberships m
JOIN user_identities AS i ON i.issuer = m.issuer AND i.subject = m.subject
JOIN user_profiles AS p ON p.id = i.profile_id
WHERE m.organization_id = $1::UUID AND m.issuer = $2 AND m.subject = $3;
