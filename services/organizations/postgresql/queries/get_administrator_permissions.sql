SELECT r.permissions FROM memberships m
JOIN organization_roles r ON r.organization_id = m.organization_id AND r.id = m.role
WHERE m.organization_id = $1::UUID AND m.issuer = $2 AND m.subject = $3 AND m.enabled;
