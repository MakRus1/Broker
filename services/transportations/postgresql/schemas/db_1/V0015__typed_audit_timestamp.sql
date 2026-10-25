CREATE TYPE public.transportation_event_v2 AS (
    id BIGINT,
    actor TEXT,
    action TEXT,
    occurred_at TIMESTAMPTZ,
    document JSONB
);

DROP TYPE public.transportation_event_v1;
