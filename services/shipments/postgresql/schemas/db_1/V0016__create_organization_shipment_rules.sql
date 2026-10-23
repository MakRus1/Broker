CREATE TABLE organization_shipment_rules (
    organization_id UUID PRIMARY KEY,
    number_template TEXT NOT NULL,
    reset_number_yearly BOOLEAN NOT NULL,
    statuses TEXT[] NOT NULL,
    route_types TEXT[] NOT NULL,
    planned_containers BOOLEAN NOT NULL,
    container_types TEXT[] NOT NULL,
    revision BIGINT NOT NULL DEFAULT 1
);

CREATE TYPE public.shipment_business_rules_v1 AS (
    number_template TEXT,
    reset_number_yearly BOOLEAN,
    statuses TEXT[],
    route_types TEXT[],
    planned_containers BOOLEAN,
    container_types TEXT[]
);

CREATE TYPE public.organization_shipment_rules_v1 AS (
    rules public.shipment_business_rules_v1,
    revision BIGINT
);
