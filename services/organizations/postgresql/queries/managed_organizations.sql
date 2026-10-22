SELECT organization_id::TEXT FROM effective_memberships
WHERE issuer = $1 AND subject = $2 AND permissions @> ARRAY['organizations.manage']
ORDER BY organization_id;
