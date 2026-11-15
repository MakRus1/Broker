-- $1 — организация.
-- $2 — идентификатор начисления.
-- $3 — номер записываемой редакции.
-- $4 — идентификатор автора изменения.
-- $5 — снимок редакции в JSON.
INSERT INTO service_charge_history (
    organization_id, service_charge_id, revision, actor, snapshot
)
VALUES ($1::UUID, $2::UUID, $3::BIGINT, $4::TEXT, $5::JSONB);
