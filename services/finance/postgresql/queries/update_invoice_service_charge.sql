UPDATE invoice_service_charges
SET snapshot = $4::public.service_charge_v1
WHERE organization_id = $1::UUID
    AND invoice_id = $2::UUID
    AND service_charge_id = $3::UUID
    AND NOT released
RETURNING snapshot;
