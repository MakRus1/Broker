-- $1 — организация.
-- $2 — ожидаемая редакция (0 при создании).
-- $3 — новые правила поставок.
INSERT INTO organization_shipment_rules AS current (
    organization_id,
    number_template,
    reset_number_yearly,
    statuses,
    route_types,
    planned_containers
)
SELECT
    $1::UUID,
    rules.number_template,
    rules.reset_number_yearly,
    rules.statuses,
    rules.route_types,
    rules.planned_containers
FROM (SELECT ($3::public.shipment_business_rules_v2).*) AS rules
WHERE $2::BIGINT = 0 OR EXISTS (
    SELECT 1 FROM organization_shipment_rules WHERE organization_id = $1::UUID
)
ON CONFLICT (organization_id) DO UPDATE SET
    number_template = EXCLUDED.number_template,
    reset_number_yearly = EXCLUDED.reset_number_yearly,
    statuses = EXCLUDED.statuses,
    route_types = EXCLUDED.route_types,
    planned_containers = EXCLUDED.planned_containers,
    revision = current.revision + 1
WHERE current.revision = $2::BIGINT
RETURNING revision;
