-- $1 — организация.
-- $2 — идентификатор перевозки.
-- $3 — полный маршрут, включая исторические участки, в порядке следования.
UPDATE transportations
SET route_segments = to_jsonb($3::JSONB[])
WHERE organization_id = $1::UUID AND id = $2::UUID;
