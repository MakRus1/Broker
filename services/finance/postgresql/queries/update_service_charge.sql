UPDATE service_charges AS charge
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
FROM (SELECT ($2::public.service_charge_v1).*) AS value
WHERE charge.organization_id = $1::UUID
    AND charge.id = value.id
    AND charge.shipment_id = value.shipment_id
    AND charge.transportation_id IS NOT DISTINCT FROM value.transportation_id
    AND charge.revision = $3::BIGINT
RETURNING ROW(
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
)::public.service_charge_v1;
