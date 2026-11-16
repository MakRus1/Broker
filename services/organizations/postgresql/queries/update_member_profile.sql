-- $1 — организация, из которой редактируется профиль.
-- $2 — issuer токена пользователя.
-- $3 — subject пользователя.
-- $4 — фамилия.
-- $5 — имя.
-- $6 — отчество.
WITH changed AS (
    UPDATE user_profiles AS p
    SET display_name = concat_ws(' ', $4::TEXT, $5::TEXT, $6::TEXT),
        last_name = $4, first_name = $5, middle_name = $6
    FROM user_identities AS i
    JOIN memberships AS m USING (issuer, subject)
    WHERE m.organization_id = $1 AND m.issuer = $2 AND m.subject = $3
        AND p.id = i.profile_id
    RETURNING p.id
)
UPDATE memberships AS m
SET revision = revision + 1
FROM user_identities AS i
JOIN changed ON changed.id = i.profile_id
WHERE m.issuer = i.issuer AND m.subject = i.subject
RETURNING m.subject;
