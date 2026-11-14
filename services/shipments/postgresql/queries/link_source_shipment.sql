-- $1 — организация.
-- $2 — сервис-источник.
-- $3 — тип источника.
-- $4 — идентификатор источника.
-- $5 — идентификатор поставки.
-- $6 — исходные данные запроса.
INSERT INTO shipment_sources (organization_id, producer, source_type, source_id, shipment_id, payload)
VALUES ($1::UUID, $2, $3, $4, $5::UUID, $6::JSONB);
