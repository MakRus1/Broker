-- $1 — организация.
-- $2 — сервис-источник.
-- $3 — тип источника.
-- $4 — идентификатор источника.
SELECT shipment_id, payload FROM shipment_sources
WHERE organization_id = $1::UUID AND producer = $2 AND source_type = $3 AND source_id = $4;
