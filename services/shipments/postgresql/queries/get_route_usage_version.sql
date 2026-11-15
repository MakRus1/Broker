-- $1 — организация.
-- $2 — идентификатор поставки.
SELECT route_usage_version
FROM shipments
WHERE organization_id = $1::UUID AND id = $2::UUID;
