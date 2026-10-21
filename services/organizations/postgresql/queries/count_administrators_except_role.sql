SELECT count(*) FROM memberships m
JOIN organization_roles r ON r.organization_id = m.organization_id AND r.id = m.role
WHERE m.organization_id = $1::UUID AND m.enabled AND m.role <> $2
  AND r.permissions @> ARRAY['organizations.manage'];
