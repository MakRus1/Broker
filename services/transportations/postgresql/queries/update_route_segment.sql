-- $1 — организация.
-- $2 — идентификатор перевозки.
-- $3 — идентификатор участка маршрута.
-- $4 — данные участка маршрута.
UPDATE route_segments
SET document = $4::JSONB
WHERE organization_id = $1::UUID AND transportation_id = $2::UUID AND id = $3::UUID;
