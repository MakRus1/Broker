CREATE TYPE public.transportation_event_v1 AS (
    id BIGINT,
    actor TEXT,
    action TEXT,
    occurred_at TEXT,
    document JSONB
);
