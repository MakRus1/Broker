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
    price_mode,
    service_date,
    invoice_currency,
    invoice_rate_millionths,
    invoice_rate_date,
    invoice_rate_source,
    invoice_net_minor,
    invoice_vat_minor,
    invoice_total_minor
)::public.service_charge_v1
FROM service_charges
WHERE organization_id = $1::UUID
    AND ($2::UUID IS NULL OR shipment_id = $2::UUID)
    AND ($2::UUID IS NULL OR transportation_id IS NOT DISTINCT FROM $3::UUID)
    AND ($4::UUID IS NULL OR id > $4::UUID)
    AND ($5::BOOLEAN OR NOT archived)
ORDER BY id
LIMIT $6::BIGINT;
