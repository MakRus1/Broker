SELECT permissions FROM effective_memberships
WHERE organization_id = $1::UUID AND issuer = $2 AND subject = $3;
