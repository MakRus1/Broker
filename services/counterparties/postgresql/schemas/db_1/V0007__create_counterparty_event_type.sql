-- Тип строки аудита для чтения истории контрагентов.
CREATE TYPE public.counterparty_event_v1 AS (
    id BIGINT,
    actor TEXT,
    action TEXT,
    occurred_at TEXT,
    document JSONB,
    previous JSONB
);
