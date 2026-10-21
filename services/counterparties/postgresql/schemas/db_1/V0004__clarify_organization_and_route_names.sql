-- Уточняем терминологию, сохраняя данные и действующие ограничения.
ALTER TABLE tenant_memberships RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE counterparties RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE business_audit RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE tenant_memberships RENAME TO organization_memberships;
