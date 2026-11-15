-- $1 — организация.
-- $2 — снимок запроса ставок в JSON.
-- $3 — идентификатор автора изменения.
INSERT INTO sales_rfq_history (organization_id, id, revision, actor, snapshot)
VALUES ($1::UUID, ($2->'input'->>'id')::UUID, ($2->>'revision')::BIGINT, $3, $2::JSONB);
