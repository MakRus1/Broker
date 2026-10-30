SELECT service_charge_id
FROM invoice_service_charges
WHERE organization_id = $1::UUID
    AND service_charge_id = ANY($2::UUID[])
    AND invoice_id <> $3::UUID
    AND NOT released;
