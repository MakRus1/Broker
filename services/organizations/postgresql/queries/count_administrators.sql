SELECT count(*) FROM effective_memberships
WHERE organization_id = $1::UUID AND permissions @> ARRAY['organizations.manage'];
