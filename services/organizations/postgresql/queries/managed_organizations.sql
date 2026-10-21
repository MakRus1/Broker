SELECT m.organization_id::TEXT FROM memberships m
JOIN organization_roles r ON r.organization_id = m.organization_id AND r.id = m.role
WHERE m.issuer = $1 AND m.subject = $2 AND m.enabled AND r.permissions @> ARRAY['organizations.manage']
ORDER BY m.organization_id;
