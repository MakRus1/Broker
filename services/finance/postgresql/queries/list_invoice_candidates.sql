SELECT ROW(
    charge.id,
    charge.shipment_id,
    charge.transportation_id,
    charge.label_id,
    charge.label_name,
    charge.direction,
    charge.unit,
    charge.counterparty_id,
    charge.counterparty_name,
    charge.contract_id,
    charge.contract_number,
    charge.quantity_milli,
    charge.price_minor,
    charge.currency,
    charge.vat_mode,
    charge.vat_rate_basis_points,
    charge.net_minor,
    charge.vat_minor,
    charge.total_minor,
    charge.note,
    charge.archived,
    charge.revision,
    charge.price_mode
)::public.service_charge_v1
FROM service_charges AS charge
WHERE organization_id = $1::UUID
    AND direction = $2::TEXT
    AND counterparty_id = $3::UUID
    AND currency = $4::TEXT
    AND contract_id IS NOT DISTINCT FROM $5::UUID
    AND NOT archived
    AND ($7::UUID IS NULL OR id > $7::UUID)
    AND NOT EXISTS (
        SELECT 1 FROM invoice_service_charges AS line
        WHERE line.organization_id = charge.organization_id
            AND line.service_charge_id = charge.id
            AND NOT line.released
            AND line.invoice_id IS DISTINCT FROM $6::UUID
    )
ORDER BY id
LIMIT $8::INTEGER;
