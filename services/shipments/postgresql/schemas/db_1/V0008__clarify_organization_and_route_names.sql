-- Уточняем терминологию, сохраняя данные и действующие ограничения.
ALTER TABLE creation_operations RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE tenant_memberships RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE idempotent_results RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE business_audit RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE shipments RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE shipment_items RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE tenant_memberships RENAME TO organization_memberships;
UPDATE creation_operations SET payload = (payload - 'tenant_id') || jsonb_build_object('organization_id', payload->'tenant_id') WHERE payload ? 'tenant_id';

-- Старый composite-тип сохраняется, новое имя поля получает отдельную версию.
CREATE TYPE public.delivery_v2 AS (
    organization UUID,
    id UUID,
    lease UUID,
    payload JSONB
);
