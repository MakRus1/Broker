CREATE TABLE role_registry_defaults (
    organization_id UUID NOT NULL,
    role TEXT NOT NULL,
    registry TEXT NOT NULL,
    value JSONB NOT NULL CHECK (jsonb_typeof(value) = 'object'),
    revision BIGINT NOT NULL DEFAULT 1,
    PRIMARY KEY (organization_id, role, registry),
    FOREIGN KEY (organization_id, role) REFERENCES organization_roles(organization_id, id)
);
