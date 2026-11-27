-- $1 — организация.
-- $2 — идентификатор поставки.
-- Каждая регистрация инвалидирует параллельную проверку удаления, в том числе при повторе.
UPDATE shipments
SET route_usage_version = route_usage_version + 1
WHERE organization_id = $1::UUID AND id = $2::UUID;
