-- $1 — организация.
INSERT INTO organization_roles (organization_id, id, name, description, permissions)
VALUES
    ($1::UUID, 'administrator', 'Администратор организации', 'Управляет сотрудниками, ролями и логистическими данными',
        ARRAY['transport_locations.read', 'organizations.read_employees', 'organizations.manage', 'shipments.read', 'shipments.write', 'shipments.create', 'transportations.read', 'transportations.write', 'counterparties.read', 'counterparties.write']),
    ($1::UUID, 'operator', 'Оператор', 'Читает и изменяет логистические данные',
        ARRAY['transport_locations.read', 'organizations.read_employees', 'shipments.read', 'shipments.write', 'shipments.create', 'transportations.read', 'transportations.write', 'counterparties.read', 'counterparties.write']),
    ($1::UUID, 'viewer', 'Наблюдатель', 'Читает логистические данные',
        ARRAY['transport_locations.read', 'organizations.read_employees', 'shipments.read', 'transportations.read', 'counterparties.read']);
