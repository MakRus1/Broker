-- $1 — имя источника справочника.
-- $2 — идентификатор аренды синхронизации.
-- $3 — хеш данных источника.
-- $4 — количество импортированных записей.
-- $5 — версия данных источника (NULL, если версии нет).
-- $6 — интервал между успешными синхронизациями, в секундах.
UPDATE directory_sync
SET lease = NULL,
    lease_until = NULL,
    failure_count = 0,
    next_attempt_at = now() + $6::INTEGER * INTERVAL '1 second',
    updated_at = now(),
    source_hash = $3,
    record_count = $4,
    source_revision = $5
WHERE name = $1 AND lease = $2::UUID;
