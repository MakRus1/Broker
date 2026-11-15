-- $1 — организация.
-- $2 — идентификатор поставки.
-- $3 — идентификатор участка маршрута.
-- $4 — тип участка маршрута поставки.
-- $5 — данные участка маршрута.
INSERT INTO shipment_route_segments (organization_id, shipment_id, id, mode, document)
VALUES ($1::UUID, $2::UUID, $3::UUID, $4::TEXT, $5::JSONB);
