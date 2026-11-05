SELECT role, registry, value, revision
FROM role_registry_defaults
WHERE organization_id = $1::UUID
ORDER BY role, registry;
