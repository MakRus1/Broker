-- $1 — организация, $2 — поставка. До первой проверки версия равна нулю.
SELECT COALESCE((SELECT version FROM route_usage_checks
    WHERE organization_id = $1 AND shipment_id = $2), 0)::BIGINT;
