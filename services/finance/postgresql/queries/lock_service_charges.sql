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
WHERE organization_id = $1::UUID AND id = ANY($2::UUID[])
ORDER BY id
FOR UPDATE;
