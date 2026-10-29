-- Каждый пул захватывает сообщения своего типа; большой backlog другого типа
-- не должен просматриваться перед получением следующего сообщения.
DROP INDEX outbox_due_idx;
CREATE INDEX outbox_due_idx ON outbox (topic, available_at, id);
