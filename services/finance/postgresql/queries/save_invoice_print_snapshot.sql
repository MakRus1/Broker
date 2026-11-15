-- $1 — организация.
-- $2 — идентификатор счёта.
-- $3 — редакция счёта для печати.
-- $4 — снимок печатной формы в JSON.
INSERT INTO invoice_print_snapshots (organization_id, invoice_id, revision, snapshot)
VALUES ($1::UUID, $2::UUID, $3::BIGINT, $4::JSONB)
ON CONFLICT (organization_id, invoice_id, revision) DO NOTHING
RETURNING snapshot;
