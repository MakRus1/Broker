SELECT organization_id::TEXT
FROM memberships
WHERE issuer = $1 AND subject = $2 AND enabled AND role = 'administrator'
ORDER BY organization_id;
