-- $1 — организация.
-- $2 — идентификатор поставки.
-- $3 — идентификатор позиции контейнерного плана.
-- $4 — идентификатор операции пакетного создания.
-- $5 — порядковый номер в операции, начиная с 1.
-- $6 — код типа контейнера.
-- $7 — принадлежность контейнера.
INSERT INTO transportations (
    organization_id, shipment_id, item_id, operation_id, ordinal, container_type_code, container_ownership
)
VALUES ($1::UUID, $2::UUID, $3::UUID, $4::UUID, $5, $6, $7)
RETURNING id::TEXT;
