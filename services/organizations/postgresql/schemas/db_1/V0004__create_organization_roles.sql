CREATE TABLE organization_roles (
    organization_id UUID NOT NULL REFERENCES organizations(id),
    id TEXT NOT NULL,
    name TEXT NOT NULL,
    description TEXT NOT NULL,
    permissions TEXT[] NOT NULL,
    revision BIGINT NOT NULL DEFAULT 1,
    UNIQUE (organization_id, name),
    PRIMARY KEY (organization_id, id)
);

-- Сохраняем права уже назначенных стартовых ролей.
INSERT INTO organization_roles (organization_id, id, name, description, permissions)
SELECT o.id, r.id, r.name, r.description, r.permissions
FROM organizations o
CROSS JOIN (VALUES
    ('administrator', 'Администратор организации', 'Управляет сотрудниками, ролями и логистическими данными',
        ARRAY['organizations.manage', 'shipments.read', 'shipments.write', 'transportations.read', 'transportations.write', 'counterparties.read', 'counterparties.write']),
    ('operator', 'Оператор', 'Читает и изменяет логистические данные',
        ARRAY['shipments.read', 'shipments.write', 'transportations.read', 'transportations.write', 'counterparties.read', 'counterparties.write']),
    ('viewer', 'Наблюдатель', 'Читает логистические данные',
        ARRAY['shipments.read', 'transportations.read', 'counterparties.read'])
) AS r(id, name, description, permissions);

ALTER TABLE memberships DROP CONSTRAINT memberships_role_check;
ALTER TABLE memberships ADD CONSTRAINT memberships_role_fkey
    FOREIGN KEY (organization_id, role) REFERENCES organization_roles(organization_id, id);

CREATE TYPE public.organization_role_v1 AS (
    id TEXT,
    name TEXT,
    description TEXT,
    permissions TEXT[],
    revision BIGINT
);
