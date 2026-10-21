SELECT count(*) FROM memberships
WHERE organization_id = $1::UUID AND enabled AND role = 'administrator';
