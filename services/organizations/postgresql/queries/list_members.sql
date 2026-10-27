SELECT
    ROW(
        m.subject,
        m.username,
        m.display_name,
        m.email,
        m.role,
        m.enabled,
        m.revision,
        m.last_name,
        m.first_name,
        m.middle_name
    )::public.member_v2,
    {sort_key} AS sort_key
FROM memberships AS m
LEFT JOIN organization_roles AS r
    ON r.organization_id = m.organization_id AND r.id = m.role
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
