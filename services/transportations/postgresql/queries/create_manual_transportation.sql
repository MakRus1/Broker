-- $1 — организация.
-- $2 — идентификатор перевозки.
-- $3 — идентификатор поставки.
-- $4 — тип перевозки.
-- $5 — признак опасного груза.
-- $6 — код типа контейнера.
-- $7 — принадлежность контейнера.
-- $8 — номер контейнера.
-- $9 — фактический код ISO контейнера.
INSERT INTO transportations (
    organization_id, id, shipment_id, ordinal, type, hazardous, container_type_code, container_ownership, container_number, container_iso_code
)
VALUES ($1::UUID, $2::UUID, $3::UUID, 1, $4, $5, $6, $7, $8, $9)
RETURNING ROW(id, revision, shipment_id, item_id, operation_id, container_type_code,
    container_ownership, container_number, type, hazardous, route_segments, container_iso_code)::public.transportation_v6;
