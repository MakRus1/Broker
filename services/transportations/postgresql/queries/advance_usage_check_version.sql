-- $1 — организация, $2 — поставка, для которой запрошены используемые участки.
INSERT INTO route_usage_checks (organization_id, shipment_id, version) VALUES ($1, $2, 1)
ON CONFLICT (organization_id, shipment_id) DO UPDATE SET version = route_usage_checks.version + 1;
