SELECT d.role, d.registry, d.value, d.revision
FROM effective_memberships m
JOIN role_registry_defaults d
    ON d.organization_id = m.organization_id AND d.role = m.role
WHERE m.organization_id = $1::UUID AND m.issuer = $2 AND m.subject = $3
ORDER BY d.registry;
