-- $1 — организация.
-- $2 — ключ команды (идентификатор операции пакетного создания или создаваемой вручную перевозки).
SELECT ROW(
    payload,
    result
)::public.saved_command_v1
FROM processed_commands
WHERE organization_id = $1::UUID AND operation_id = $2::UUID;
