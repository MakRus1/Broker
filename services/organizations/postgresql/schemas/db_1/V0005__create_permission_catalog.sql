-- Каталог версии сохраняется после деактивации для отката без потери назначений.
CREATE TABLE permission_releases (
    owner TEXT NOT NULL,
    version TEXT NOT NULL,
    entries JSONB NOT NULL CHECK (jsonb_typeof(entries) = 'array'),
    active BOOLEAN NOT NULL,
    registered_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (owner, version)
);

CREATE VIEW permission_catalog AS
SELECT DISTINCT ON (entry->>'code')
    entry->>'code' AS code,
    entry->>'name' AS name,
    entry->>'group' AS group_name,
    entry->>'scope' AS scope,
    ARRAY(SELECT jsonb_array_elements_text(entry->'requires')) AS requires
FROM permission_releases r, jsonb_array_elements(r.entries) AS entry
WHERE r.active
ORDER BY entry->>'code', r.registered_at DESC, r.version;


-- Единственное место расчёта действующих прав сотрудника организации.
CREATE VIEW effective_memberships AS
SELECT m.organization_id, m.issuer, m.subject, m.role,
    ARRAY(SELECT p.code FROM permission_catalog p
          WHERE p.scope = 'organization' AND p.code = ANY(r.permissions)) AS permissions
FROM memberships m
JOIN organizations o ON o.id = m.organization_id
JOIN organization_roles r ON r.organization_id = m.organization_id AND r.id = m.role
WHERE m.enabled AND o.enabled;
