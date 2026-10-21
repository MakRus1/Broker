SELECT organization_id::TEXT
FROM organization_memberships
WHERE issuer = $1
  AND subject = $2
  AND ($3::UUID IS NULL OR organization_id = $3::UUID)
LIMIT 2;
