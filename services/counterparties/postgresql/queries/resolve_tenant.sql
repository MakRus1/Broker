SELECT tenant_id::TEXT
FROM tenant_memberships
WHERE issuer = $1
  AND subject = $2
  AND ($3::UUID IS NULL OR tenant_id = $3::UUID)
LIMIT 2;
