SELECT o.id::TEXT, r.permissions FROM organizations o
JOIN memberships m ON m.organization_id = o.id
JOIN organization_roles r ON r.organization_id = m.organization_id AND r.id = m.role
WHERE m.issuer = $1 AND m.subject = $2 AND m.enabled AND o.enabled
  AND ($3::UUID IS NULL OR o.id = $3::UUID)
ORDER BY o.id LIMIT 2;
