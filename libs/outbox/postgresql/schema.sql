-- Стандартная outbox v1. Источник: libs/outbox/postgresql/schema.sql.
-- Сгенерированная миграция остаётся неизменной после применения.
CREATE TABLE outbox (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    topic TEXT NOT NULL,
    message_key TEXT NOT NULL,
    payload JSONB NOT NULL,
    generation BIGINT NOT NULL DEFAULT 1,
    attempts INTEGER NOT NULL DEFAULT 0,
    available_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    lease_token UUID,
    UNIQUE (topic, message_key)
);

-- available_at — следующая попытка или срок истечения текущего захвата.
CREATE INDEX outbox_due_idx ON outbox (available_at, id);
