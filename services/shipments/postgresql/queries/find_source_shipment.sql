-- $1 — организация.
-- $2 — сервис-источник.
-- $3 — тип источника.
-- $4 — UUID источника сервисной команды.
-- $5 — ключ идемпотентности ручного создания.
SELECT shipment_id, payload FROM shipment_sources
WHERE organization_id = $1::UUID AND producer = $2 AND source_type = $3
    AND (source_id = $4::UUID OR manual_idempotency_key = $5);
