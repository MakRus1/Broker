-- Старое поле сохраняется для совместимости с предыдущей версией сервиса.
ALTER TABLE organization_shipment_rules ALTER COLUMN container_types SET DEFAULT '{}';

CREATE TYPE public.shipment_business_rules_v2 AS (
    number_template TEXT,
    reset_number_yearly BOOLEAN,
    statuses TEXT[],
    route_types TEXT[],
    planned_containers BOOLEAN
);

CREATE TYPE public.organization_shipment_rules_v2 AS (
    rules public.shipment_business_rules_v2,
    revision BIGINT
);
