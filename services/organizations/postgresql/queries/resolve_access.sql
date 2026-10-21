SELECT o.id::TEXT, m.role
FROM organizations o
JOIN memberships m ON m.organization_id = o.id
WHERE m.issuer = $1 AND m.subject = $2 AND m.enabled AND o.enabled
  AND ($3::UUID IS NULL OR o.id = $3::UUID)
ORDER BY o.id
LIMIT 2;
