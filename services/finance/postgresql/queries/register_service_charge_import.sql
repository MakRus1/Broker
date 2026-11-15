-- $1 — организация.
-- $2 — сервис-источник переноса.
-- $3 — идентификатор команды переноса.
-- $4 — данные команды переноса в JSON.
INSERT INTO service_charge_imports (organization_id, producer, id, document)
VALUES ($1::UUID, $2, $3::UUID, $4::JSONB)
ON CONFLICT (organization_id, producer, id) DO NOTHING
RETURNING id;
