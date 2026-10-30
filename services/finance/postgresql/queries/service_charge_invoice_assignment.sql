SELECT invoice_id
FROM invoice_service_charges
WHERE organization_id = $1::UUID AND service_charge_id = $2::UUID AND NOT released;
