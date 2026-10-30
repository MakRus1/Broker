SELECT line.service_charge_id, invoice.id, invoice.number, invoice.direction, invoice.invoice_date
FROM invoice_service_charges AS line
JOIN invoices AS invoice
    ON invoice.organization_id = line.organization_id AND invoice.id = line.invoice_id
JOIN service_charges AS charge
    ON charge.organization_id = line.organization_id AND charge.id = line.service_charge_id
WHERE line.organization_id = $1::UUID
    AND charge.id = ANY($2::UUID[])
    AND NOT line.released;
