-- $1 — организация.
SELECT ROW(
    ROW(number_template, reset_number_yearly, statuses, route_types, planned_containers)::public.shipment_business_rules_v2,
    revision
)::public.organization_shipment_rules_v2
FROM organization_shipment_rules
WHERE organization_id = $1::UUID;
