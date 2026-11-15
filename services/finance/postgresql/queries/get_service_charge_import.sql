-- $1 — организация.
-- $2 — сервис-источник переноса.
-- $3 — идентификатор команды переноса.
SELECT document FROM service_charge_imports
WHERE organization_id = $1::UUID AND producer = $2 AND id = $3::UUID;
