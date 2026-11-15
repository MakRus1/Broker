-- $1 — организация.
-- $2 — идентификатор договора.
-- $3 — номер записываемой редакции.
-- $4 — идентификатор автора изменения.
-- $5 — снимок редакции договора в JSON.
INSERT INTO contract_history (
    organization_id, contract_id, revision, actor, snapshot
)
VALUES ($1::UUID, $2::UUID, $3::BIGINT, $4::TEXT, $5::JSONB);
