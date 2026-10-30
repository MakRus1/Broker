UPDATE financial_entries AS entry
SET
    label_id = value.label_id,
    label_name = value.label_name,
    direction = value.direction,
    unit = value.unit,
    counterparty_id = value.counterparty_id,
    counterparty_name = value.counterparty_name,
    contract_id = value.contract_id,
    contract_number = value.contract_number,
    quantity_milli = value.quantity_milli,
    price_minor = value.price_minor,
    currency = value.currency,
    vat_mode = value.vat_mode,
    vat_rate_basis_points = value.vat_rate_basis_points,
    net_minor = value.net_minor,
    vat_minor = value.vat_minor,
    total_minor = value.total_minor,
    note = value.note,
    archived = value.archived,
    revision = value.revision,
    price_mode = value.price_mode
FROM (SELECT ($2::public.financial_entry_v1).*) AS value
WHERE entry.organization_id = $1::UUID
    AND entry.id = value.id
    AND entry.shipment_id = value.shipment_id
    AND entry.transportation_id IS NOT DISTINCT FROM value.transportation_id
    AND entry.revision = $3::BIGINT
RETURNING ROW(
    entry.id,
    entry.shipment_id,
    entry.transportation_id,
    entry.label_id,
    entry.label_name,
    entry.direction,
    entry.unit,
    entry.counterparty_id,
    entry.counterparty_name,
    entry.contract_id,
    entry.contract_number,
    entry.quantity_milli,
    entry.price_minor,
    entry.currency,
    entry.vat_mode,
    entry.vat_rate_basis_points,
    entry.net_minor,
    entry.vat_minor,
    entry.total_minor,
    entry.note,
    entry.archived,
    entry.revision,
    entry.price_mode
)::public.financial_entry_v1;
