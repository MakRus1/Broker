-- $1 — организация.
-- $2 — идентификатор операции пакетного создания.
-- $3 — результат команды.
UPDATE processed_commands
SET result = $3::JSONB
WHERE organization_id = $1::UUID AND operation_id = $2::UUID;
