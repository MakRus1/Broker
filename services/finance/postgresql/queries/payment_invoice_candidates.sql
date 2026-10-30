SELECT
    ROW(
        invoice.id,
        invoice.direction,
        invoice.number,
        invoice.invoice_date,
        invoice.due_date,
        invoice.own_company_id,
        invoice.own_company_name,
        invoice.counterparty_id,
        invoice.counterparty_name,
        invoice.own_bank_account_id,
        invoice.counterparty_bank_account_id,
        invoice.contract_id,
        invoice.contract_number,
        invoice.currency,
        invoice.note,
        invoice.archived,
        invoice.revision,
        invoice.net_minor,
        invoice.vat_minor,
        invoice.total_minor
    )::public.invoice_v1,
    COALESCE(paid.amount_minor, 0)::BIGINT
FROM invoices AS invoice
LEFT JOIN LATERAL (
    SELECT SUM(allocation.amount_minor) AS amount_minor
    FROM payment_allocations AS allocation
    JOIN payments AS payment
        ON payment.organization_id = allocation.organization_id AND payment.id = allocation.payment_id
    WHERE allocation.organization_id = invoice.organization_id
        AND allocation.invoice_id = invoice.id AND NOT payment.archived
        AND ($9::UUID IS NULL OR allocation.payment_id <> $9::UUID)
) AS paid ON TRUE
WHERE invoice.organization_id = $1::UUID
    AND direction = $2::TEXT
    AND own_company_id = $3::UUID AND counterparty_id = $4::UUID
    AND currency = $5::TEXT AND NOT archived
    AND ($6::TEXT IS NULL OR strpos(lower(number), lower($6::TEXT)) > 0)
    AND ($7::UUID IS NULL OR invoice.id > $7::UUID)
ORDER BY invoice.id
LIMIT $8::INTEGER;
