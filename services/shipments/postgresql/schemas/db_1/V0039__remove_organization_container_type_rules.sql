ALTER TABLE organization_shipment_rules DROP COLUMN container_types;

DROP TYPE public.organization_shipment_rules_v1;
DROP TYPE public.shipment_business_rules_v1;

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
