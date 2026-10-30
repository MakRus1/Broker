SELECT ROW(
    id,
    shipment_id,
    transportation_id,
    label_id,
    label_name,
    direction,
    unit,
    counterparty_id,
    counterparty_name,
    contract_id,
    contract_number,
    quantity_milli,
    price_minor,
    currency,
    vat_mode,
    vat_rate_basis_points,
    net_minor,
    vat_minor,
    total_minor,
    note,
    archived,
    revision,
    price_mode
)::public.service_charge_v1
FROM service_charges
WHERE organization_id = $1::UUID
    AND shipment_id = $2::UUID
    AND transportation_id IS NOT DISTINCT FROM $3::UUID
    AND ($4::UUID IS NULL OR id > $4::UUID)
    AND ($5::BOOLEAN OR NOT archived)
ORDER BY id
LIMIT $6::BIGINT;
