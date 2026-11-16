SELECT
    ROW(
        m.subject,
        m.username,
        p.display_name,
        m.email,
        m.role,
        m.enabled,
        m.revision,
        p.last_name,
        p.first_name,
        p.middle_name
    )::public.member_v2,
    {sort_key} AS sort_key
FROM memberships AS m
JOIN user_identities AS i ON i.issuer = m.issuer AND i.subject = m.subject
JOIN user_profiles AS p ON p.id = i.profile_id
LEFT JOIN organization_roles AS r
    ON r.organization_id = m.organization_id AND r.id = m.role
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
