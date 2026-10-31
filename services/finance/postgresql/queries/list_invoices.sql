SELECT ROW(
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
    payment_total.paid_minor,
    settlement.outstanding_minor,
    settlement.payment_status,
    settlement.overdue_days,
    ({sort_key})::TEXT AS text_key,
    {date_key} AS date_key
FROM invoices AS invoice
LEFT JOIN LATERAL (
    SELECT COALESCE(SUM(allocation.amount_minor), 0)::BIGINT AS paid_minor
    FROM payment_allocations AS allocation
    JOIN payments AS payment
        ON payment.organization_id = allocation.organization_id
        AND payment.id = allocation.payment_id
    WHERE allocation.organization_id = invoice.organization_id
        AND allocation.invoice_id = invoice.id
        AND NOT payment.archived
) AS payment_total ON TRUE
CROSS JOIN LATERAL (
    SELECT
        invoice.total_minor - payment_total.paid_minor AS outstanding_minor,
        CASE
            WHEN payment_total.paid_minor >= invoice.total_minor THEN 'paid'
            WHEN payment_total.paid_minor > 0 THEN 'partial'
            ELSE 'unpaid'
        END AS payment_status,
        CASE
            WHEN NOT invoice.archived AND invoice.total_minor > payment_total.paid_minor
                AND invoice.due_date < CURRENT_DATE
            THEN CURRENT_DATE - invoice.due_date
            ELSE 0
        END AS overdue_days
) AS settlement
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
