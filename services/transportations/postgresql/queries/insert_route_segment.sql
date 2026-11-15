-- $1 — организация.
-- $2 — идентификатор перевозки.
-- $3 — идентификатор участка маршрута.
-- $4 — позиция участка в маршруте, начиная с 0.
-- $5 — данные участка маршрута.
INSERT INTO route_segments (organization_id, transportation_id, id, position, document)
VALUES ($1::UUID, $2::UUID, $3::UUID, $4, $5::JSONB)
ON CONFLICT (organization_id, transportation_id, id)
DO UPDATE SET position = EXCLUDED.position, document = EXCLUDED.document;
