ALTER TABLE transportations
    ALTER COLUMN item_id DROP NOT NULL,
    ALTER COLUMN operation_id DROP NOT NULL,
    ALTER COLUMN container_type_code DROP NOT NULL,
    ALTER COLUMN container_ownership DROP NOT NULL,
    ADD COLUMN type TEXT NOT NULL DEFAULT 'container',
    ADD COLUMN hazardous BOOLEAN NOT NULL DEFAULT FALSE;

CREATE TYPE public.transportation_v3 AS (
    id UUID,
    revision BIGINT,
    shipment_id UUID,
    item_id UUID,
    operation_id UUID,
    container_type TEXT,
    ownership TEXT,
    number TEXT,
    type TEXT,
    hazardous BOOLEAN
);
