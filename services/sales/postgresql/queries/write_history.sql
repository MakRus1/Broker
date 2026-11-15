-- $1 — организация.
-- $2 — вид сущности.
-- $3 — идентификатор сущности.
-- $4 — номер записываемой редакции.
-- $5 — идентификатор автора изменения.
-- $6 — снимок записываемой редакции в JSON.
INSERT INTO sales_history (organization_id, entity, id, revision, actor, snapshot)
VALUES ($1::UUID, $2, $3::UUID, $4, $5, $6::JSONB);
