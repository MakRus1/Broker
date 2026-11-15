-- $1 — организация.
-- $2 — сервис-источник.
-- $3 — тип источника.
-- $4 — UUID источника сервисной команды.
-- $5 — ключ идемпотентности ручного создания.
-- $6 — идентификатор поставки.
-- $7 — исходные данные запроса.
INSERT INTO shipment_sources (
    organization_id, producer, source_type, source_id, manual_idempotency_key, shipment_id, payload
)
VALUES ($1::UUID, $2, $3, $4::UUID, $5, $6::UUID, $7::JSONB);
