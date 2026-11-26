ALTER TABLE transportations ADD COLUMN container_iso_code TEXT
    CHECK (container_iso_code IS NULL OR (
        type = 'container' AND container_type_code IS NOT NULL
        AND container_iso_code ~ '^[A-Z0-9]{4}$'
    ));

CREATE TYPE public.transportation_v6 AS (
    id UUID,
    revision BIGINT,
    shipment_id UUID,
    item_id UUID,
    operation_id UUID,
    container_type TEXT,
    ownership TEXT,
    container_number TEXT,
    type TEXT,
    hazardous BOOLEAN,
    route_segments JSONB,
    container_iso_code TEXT
);
