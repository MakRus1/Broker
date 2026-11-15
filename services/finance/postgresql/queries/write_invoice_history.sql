-- $1 — организация.
-- $2 — идентификатор счёта.
-- $3 — номер записываемой редакции.
-- $4 — идентификатор автора изменения.
-- $5 — снимок редакции в JSON.
INSERT INTO invoice_history (organization_id, invoice_id, revision, actor, snapshot)
VALUES ($1::UUID, $2::UUID, $3::BIGINT, $4::TEXT, $5::JSONB);
