-- $1 — issuer токена пользователя.
-- $2 — subject пользователя.
-- $3 — идентификатор организации (NULL возвращает доступ ко всем доступным организациям).
SELECT organization_id::TEXT, permissions FROM effective_memberships
WHERE issuer = $1 AND subject = $2
  AND ($3::UUID IS NULL OR organization_id = $3::UUID)
ORDER BY organization_id LIMIT 2;
