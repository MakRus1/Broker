-- $1 — организация, $2 — перевозка, $3 — ожидаемая редакция, $4 — подготовленный маршрут.
UPDATE transportations
SET route_segments = to_jsonb($4::JSONB[]), revision = revision + 1
WHERE organization_id = $1 AND id = $2 AND revision = $3
RETURNING revision;
