-- $1 — организация.
-- $2 — действие, в пределах которого действует ключ.
-- $3 — ключ идемпотентности.
-- $4 — данные исходного запроса.
-- $5 — сохранённый результат запроса.
INSERT INTO idempotent_results (organization_id, action, key, request, result)
VALUES ($1::UUID, $2, $3, $4::JSONB, $5::JSONB);
