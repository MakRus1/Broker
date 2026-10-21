SELECT count(*) FROM memberships
WHERE organization_id = $1::UUID AND role = $2 AND enabled;
