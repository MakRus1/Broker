-- Уточняем терминологию, сохраняя данные и действующие ограничения.
ALTER TABLE tenant_memberships RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE processed_commands RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE transportations RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE transportation_legs RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE business_audit RENAME COLUMN tenant_id TO organization_id;
ALTER TABLE tenant_memberships RENAME TO organization_memberships;
ALTER TABLE transportation_legs RENAME TO route_segments;

CREATE TYPE public.route_segment_v1 AS (
    transportation_id UUID,
    document JSONB
);

-- Меняем только JSON-ключи; кавычки внутри строковых значений экранированы JSONB.
-- Переносим снимки истории и inbox, чтобы повторы команд остались сопоставимыми.
UPDATE business_audit SET document = (replace(
    replace(replace(document::text, '"legs":', '"route_segments":'), '"leg_id":', '"route_segment_id":'), '"tenant_id":', '"organization_id":'))::jsonb;
UPDATE processed_commands SET payload = (replace(
    replace(replace(payload::text, '"legs":', '"route_segments":'), '"leg_id":', '"route_segment_id":'), '"tenant_id":', '"organization_id":'))::jsonb;
UPDATE processed_commands SET result = (replace(
    replace(replace(result::text, '"legs":', '"route_segments":'), '"leg_id":', '"route_segment_id":'), '"tenant_id":', '"organization_id":'))::jsonb;
