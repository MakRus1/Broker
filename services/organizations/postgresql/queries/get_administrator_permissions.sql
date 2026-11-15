-- $1 — организация.
-- $2 — issuer токена пользователя.
-- $3 — subject пользователя.
SELECT permissions FROM effective_memberships
WHERE organization_id = $1::UUID AND issuer = $2 AND subject = $3;
