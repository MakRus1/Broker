CREATE TYPE public.transportation_v1 AS (
    id UUID,
    revision BIGINT,
    shipment_id UUID,
    item_id UUID,
    operation_id UUID,
    container_type TEXT,
    ownership TEXT
);

CREATE TYPE public.saved_command_v1 AS (
    payload JSONB,
    result JSONB
);

CREATE TYPE public.transportation_leg_v1 AS (
    transportation_id UUID,
    document JSONB
);
