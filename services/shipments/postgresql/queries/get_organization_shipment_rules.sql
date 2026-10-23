SELECT ROW(
    ROW(number_template, reset_number_yearly, statuses, route_types, planned_containers, container_types)::public.shipment_business_rules_v1,
    revision
)::public.organization_shipment_rules_v1
FROM organization_shipment_rules
WHERE organization_id = $1::UUID;
