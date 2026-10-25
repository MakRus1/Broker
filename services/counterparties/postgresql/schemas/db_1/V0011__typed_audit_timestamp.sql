CREATE TYPE public.counterparty_event_v2 AS (
    id BIGINT,
    actor TEXT,
    action TEXT,
    occurred_at TIMESTAMPTZ,
    document JSONB,
    previous JSONB
);

DROP TYPE public.counterparty_event_v1;
