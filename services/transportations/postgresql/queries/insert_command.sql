-- $1 — организация.
-- $2 — ключ команды (идентификатор операции пакетного создания или создаваемой вручную перевозки).
-- $3 — данные команды.
-- $4 — результат команды.
INSERT INTO processed_commands (organization_id, operation_id, payload, result)
VALUES ($1::UUID, $2::UUID, $3::JSONB, $4::JSONB);
