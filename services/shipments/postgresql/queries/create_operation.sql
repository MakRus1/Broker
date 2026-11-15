-- $1 — организация.
-- $2 — идентификатор операции создания перевозок.
-- $3 — идентификатор позиции контейнерного плана.
-- $4 — количество создаваемых перевозок.
INSERT INTO creation_operations (organization_id, id, item_id, quantity)
VALUES ($1::UUID, $2::UUID, $3::UUID, $4);
