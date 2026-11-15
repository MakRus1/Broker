-- $1 — идентификатор клиента-отправителя события.
-- $2 — организация.
-- $3 — идентификатор события.
-- $4 — ожидаемый тип события.
-- $5 — ожидаемая версия формата события.
-- $6 — ожидаемые данные события в JSON.
SELECT event_type = $4 AND event_version = $5 AND payload = $6::JSONB
FROM notification_events
WHERE producer = $1
    AND organization_id = $2::UUID
    AND event_id = $3;
