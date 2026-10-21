-- Типы результатов запросов. Изменение контракта оформляется новым *_v2.
CREATE TYPE public.shipment_v1 AS (
    id UUID,
    revision BIGINT,
    number TEXT,
    customer_id UUID,
    reference TEXT
);

CREATE TYPE public.shipment_item_v1 AS (
    id UUID,
    revision BIGINT,
    shipment_id UUID,
    container_type TEXT,
    ownership TEXT,
    planned BIGINT
);

CREATE TYPE public.quantity_totals_v1 AS (
    created BIGINT,
    pending BIGINT
);

CREATE TYPE public.item_totals_v1 AS (
    item_id UUID,
    created BIGINT,
    pending BIGINT
);

CREATE TYPE public.creation_operation_v1 AS (
    id UUID,
    item_id UUID,
    quantity BIGINT,
    status TEXT,
    result JSONB
);

CREATE TYPE public.idempotent_result_v1 AS (
    request JSONB,
    result JSONB
);

CREATE TYPE public.delivery_v1 AS (
    tenant UUID,
    id UUID,
    lease UUID,
    payload JSONB
);
