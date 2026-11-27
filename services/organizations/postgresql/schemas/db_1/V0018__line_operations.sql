ALTER TABLE organizations ADD COLUMN line_operations_enabled BOOLEAN NOT NULL DEFAULT FALSE;
CREATE TYPE public.organization_v2 AS (
    id UUID, name TEXT, enabled BOOLEAN, revision BIGINT, line_operations_enabled BOOLEAN
);
UPDATE organization_roles SET permissions = array_append(permissions, 'shipments.read_voyages'),
    revision = revision + 1
WHERE id = 'administrator' AND 'shipments.read' = ANY(permissions)
    AND NOT ('shipments.read_voyages' = ANY(permissions));

CREATE OR REPLACE VIEW effective_memberships AS
SELECT m.organization_id, m.issuer, m.subject, m.role,
    ARRAY(SELECT p.code FROM permission_catalog p
          WHERE p.scope = 'organization' AND p.code = ANY(r.permissions)
            AND (p.code <> 'shipments.read_voyages' OR o.line_operations_enabled)) AS permissions
FROM memberships m
JOIN organizations o ON o.id = m.organization_id
JOIN organization_roles r ON r.organization_id = m.organization_id AND r.id = m.role
WHERE m.enabled AND o.enabled;
