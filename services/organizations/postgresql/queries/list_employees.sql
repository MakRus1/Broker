-- $1 — организация.
-- $2 — subject пользователя (NULL отключает фильтр).
-- $3 — часть отображаемого имени (NULL отключает фильтр).
-- $4 — максимальное число сотрудников.
SELECT ROW(m.subject, p.display_name)::public.organization_employee_v1
FROM memberships AS m
JOIN user_identities AS i ON i.issuer = m.issuer AND i.subject = m.subject
JOIN user_profiles AS p ON p.id = i.profile_id
WHERE m.organization_id = $1::UUID AND m.enabled
    AND ($2::TEXT IS NULL OR m.subject = $2)
    AND ($3::TEXT IS NULL OR p.display_name ILIKE '%' || $3 || '%')
ORDER BY p.display_name, m.subject
LIMIT $4;
