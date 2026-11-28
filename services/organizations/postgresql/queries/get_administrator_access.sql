-- $1 — организация.
-- $2 — issuer токена пользователя.
-- $3 — subject пользователя.
-- Нет строки — организации нет. Пустой массив — пользователь не состоит в организации.
SELECT COALESCE(membership.permissions, ARRAY[]::TEXT[])
FROM organizations AS organization
LEFT JOIN effective_memberships AS membership
    ON membership.organization_id = organization.id
    AND membership.issuer = $2
    AND membership.subject = $3
WHERE organization.id = $1::UUID;
