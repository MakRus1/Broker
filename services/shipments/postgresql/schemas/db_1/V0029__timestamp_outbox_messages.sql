-- Для существующих сообщений известен только момент введения метрики возраста.
-- available_at содержит также lease/повторы и не является временем постановки.
ALTER TABLE outbox ADD COLUMN created_at TIMESTAMPTZ NOT NULL DEFAULT now();
