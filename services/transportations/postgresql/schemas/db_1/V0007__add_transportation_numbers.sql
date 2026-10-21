ALTER TABLE transportations ADD COLUMN number BIGINT GENERATED ALWAYS AS IDENTITY;
CREATE UNIQUE INDEX transportations_number_unique ON transportations (number);

CREATE TYPE public.transportation_v2 AS (
    id UUID,
    revision BIGINT,
    shipment_id UUID,
    item_id UUID,
    operation_id UUID,
    container_type TEXT,
    ownership TEXT,
    number TEXT
);
