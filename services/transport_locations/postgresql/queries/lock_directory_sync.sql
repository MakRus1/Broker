-- $1 — имя источника справочника.
-- $2 — идентификатор аренды синхронизации.
SELECT COALESCE(record_count, 0)
FROM directory_sync
WHERE name = $1 AND lease = $2::UUID AND lease_until > now()
FOR UPDATE;
