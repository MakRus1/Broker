SELECT document FROM service_charge_imports
WHERE organization_id = $1::UUID AND producer = $2 AND id = $3::UUID;
