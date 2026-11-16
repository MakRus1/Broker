-- $1 — issuer токена текущего пользователя.
-- $2 — subject текущего пользователя.
SELECT p.display_name
FROM user_identities AS i
JOIN user_profiles AS p ON p.id = i.profile_id
WHERE i.issuer = $1 AND i.subject = $2;
