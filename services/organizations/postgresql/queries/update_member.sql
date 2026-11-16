-- $1 — организация.
-- $2 — issuer токена пользователя.
-- $3 — subject пользователя.
-- $4 — новый идентификатор роли.
-- $5 — новый признак активности участника.
UPDATE memberships AS m
SET role = $4, enabled = $5, revision = revision + 1
FROM user_identities AS i JOIN user_profiles AS p ON p.id = i.profile_id
WHERE m.organization_id = $1::UUID AND m.issuer = $2 AND m.subject = $3
    AND i.issuer = m.issuer AND i.subject = m.subject
RETURNING ROW(m.subject, m.username, p.display_name, m.email, m.role, m.enabled, m.revision, p.last_name, p.first_name, p.middle_name)::public.member_v2;
