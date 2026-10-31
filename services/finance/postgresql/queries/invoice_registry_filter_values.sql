SELECT ({key})::TEXT AS value, COALESCE(MIN(({label})::TEXT), '') AS label
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
{facet_join}
WHERE {where}
GROUP BY {key}
ORDER BY {key}
LIMIT {limit};
