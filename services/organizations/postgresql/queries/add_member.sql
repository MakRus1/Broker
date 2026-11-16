-- $1 — организация.
-- $2 — issuer токена пользователя.
-- $3 — данные участника организации, включая subject и роль.
WITH input AS (
    SELECT ($3::public.member_v2).*
), identity AS (
    -- При конкурентном добавлении в две организации обе операции получают один profile_id.
    INSERT INTO user_identities AS i (issuer, subject, profile_id)
    SELECT $2, subject, gen_random_uuid() FROM input
    ON CONFLICT (issuer, subject) DO UPDATE SET profile_id = i.profile_id
    RETURNING profile_id
), profile AS (
    INSERT INTO user_profiles (id, display_name, last_name, first_name, middle_name)
    SELECT profile_id, display_name, last_name, first_name, middle_name
    FROM identity CROSS JOIN input
    ON CONFLICT (id) DO NOTHING
)
INSERT INTO memberships (organization_id, issuer, subject, username, email, role)
SELECT $1::UUID, $2, subject, username, email, role FROM input;
