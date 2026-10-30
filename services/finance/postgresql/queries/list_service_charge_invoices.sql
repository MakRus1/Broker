SELECT line.service_charge_id, invoice.id, invoice.number, invoice.direction, invoice.invoice_date,
    EXISTS (
        SELECT 1
        FROM payment_allocations AS allocation
        JOIN payments AS payment
            ON payment.organization_id = allocation.organization_id AND payment.id = allocation.payment_id
        WHERE allocation.organization_id = invoice.organization_id
            AND allocation.invoice_id = invoice.id AND NOT payment.archived
    ) AS read_only
FROM invoice_service_charges AS line
JOIN invoices AS invoice
    ON invoice.organization_id = line.organization_id AND invoice.id = line.invoice_id
JOIN service_charges AS charge
    ON charge.organization_id = line.organization_id AND charge.id = line.service_charge_id
WHERE line.organization_id = $1::UUID
    AND charge.id = ANY($2::UUID[])
    AND NOT line.released;
