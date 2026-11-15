-- $1 — идентификатор клиента-отправителя события.
-- $2 — организация.
-- $3 — идентификатор события.
-- $4 — ожидаемый тип события.
-- $5 — ожидаемая версия формата события.
SELECT payload
FROM notification_events
WHERE producer = $1
    AND organization_id = $2::UUID
    AND event_id = $3
    AND event_type = $4
    AND event_version = $5;
