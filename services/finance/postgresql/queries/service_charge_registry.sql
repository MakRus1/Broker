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
    charge.price_mode,
    charge.service_date,
    charge.invoice_currency,
    charge.invoice_rate_millionths,
    charge.invoice_rate_date,
    charge.invoice_rate_source,
    charge.invoice_net_minor,
    charge.invoice_vat_minor,
    charge.invoice_total_minor
)::public.service_charge_v1,
    ({sort_key})::TEXT AS text_key,
    {date_key} AS date_key
FROM service_charges AS charge
LEFT JOIN (
    SELECT line.organization_id AS invoice_organization, line.service_charge_id,
        invoice.id AS invoice_id, invoice.number AS invoice_number
    FROM invoice_service_charges AS line
    JOIN invoices AS invoice
        ON invoice.organization_id = line.organization_id AND invoice.id = line.invoice_id
    WHERE NOT line.released
) AS invoice_link
    ON invoice_link.invoice_organization = charge.organization_id
    AND invoice_link.service_charge_id = charge.id
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
