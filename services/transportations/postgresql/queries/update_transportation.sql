-- $1 — организация.
-- $2 — идентификатор перевозки.
-- $3 — тип перевозки.
-- $4 — признак опасного груза.
-- $5 — код типа контейнера.
-- $6 — принадлежность контейнера.
-- $7 — номер контейнера.
UPDATE transportations
SET type = $3,
    hazardous = $4,
    container_type_code = $5,
    container_ownership = $6,
    container_number = $7,
    revision = revision + 1
WHERE organization_id = $1::UUID AND id = $2::UUID
RETURNING ROW(id, revision, shipment_id, item_id, operation_id, container_type_code,
    container_ownership, container_number, type, hazardous, route_segments)::public.transportation_v5;
