-- $1 — организация, $2 — контрагент, $3 — своё юрлицо, $4 — договор, $5 — валюта.
-- Договор ограничивает только счета: нераспределённые платежи ещё не относятся к договору.
WITH selected_invoices AS (
    SELECT id, direction, currency, due_date, total_minor
    FROM invoices
    WHERE organization_id = $1::UUID
        AND counterparty_id = $2::UUID
        AND ($3::UUID IS NULL OR own_company_id = $3::UUID)
        AND ($4::UUID IS NULL OR contract_id = $4::UUID)
        AND ($5::TEXT IS NULL OR currency = $5::TEXT)
        AND NOT archived
), selected_payments AS (
    SELECT id, kind, currency, amount_minor
    FROM payments
    WHERE organization_id = $1::UUID
        AND counterparty_id = $2::UUID
        AND ($3::UUID IS NULL OR own_company_id = $3::UUID)
        AND ($5::TEXT IS NULL OR currency = $5::TEXT)
        AND NOT archived
), invoice_allocations AS (
    SELECT allocation.invoice_id, SUM(allocation.amount_minor) AS paid_minor
    FROM payment_allocations AS allocation
    JOIN selected_invoices AS invoice ON invoice.id = allocation.invoice_id
    JOIN selected_payments AS payment ON payment.id = allocation.payment_id
    WHERE allocation.organization_id = $1::UUID
    GROUP BY allocation.invoice_id
), payment_allocations_total AS (
    SELECT allocation.payment_id, SUM(allocation.amount_minor) AS allocated_minor
    FROM payment_allocations AS allocation
    JOIN selected_payments AS payment ON payment.id = allocation.payment_id
    WHERE allocation.organization_id = $1::UUID
    GROUP BY allocation.payment_id
), invoice_balances AS (
    SELECT
        invoice.currency,
        invoice.direction,
        invoice.due_date,
        invoice.total_minor - COALESCE(allocation.paid_minor, 0) AS outstanding_minor
    FROM selected_invoices AS invoice
    LEFT JOIN invoice_allocations AS allocation ON allocation.invoice_id = invoice.id
), amounts AS (
    SELECT
        currency,
        CASE WHEN direction = 'outgoing' THEN outstanding_minor ELSE 0 END AS receivable_minor,
        CASE WHEN direction = 'incoming' THEN outstanding_minor ELSE 0 END AS payable_minor,
        CASE WHEN direction = 'outgoing' AND due_date < CURRENT_DATE
            THEN outstanding_minor ELSE 0 END AS overdue_receivable_minor,
        CASE WHEN direction = 'incoming' AND due_date < CURRENT_DATE
            THEN outstanding_minor ELSE 0 END AS overdue_payable_minor,
        0 AS unallocated_receipts_minor,
        0 AS unallocated_disbursements_minor
    FROM invoice_balances

    UNION ALL

    SELECT
        payment.currency,
        0, 0, 0, 0,
        CASE WHEN payment.kind = 'receipt'
            THEN payment.amount_minor - COALESCE(allocation.allocated_minor, 0) ELSE 0 END,
        CASE WHEN payment.kind = 'disbursement'
            THEN payment.amount_minor - COALESCE(allocation.allocated_minor, 0) ELSE 0 END
    FROM selected_payments AS payment
    LEFT JOIN payment_allocations_total AS allocation ON allocation.payment_id = payment.id
)
SELECT
    currency,
    SUM(receivable_minor)::TEXT,
    SUM(payable_minor)::TEXT,
    SUM(overdue_receivable_minor)::TEXT,
    SUM(overdue_payable_minor)::TEXT,
    SUM(unallocated_receipts_minor)::TEXT,
    SUM(unallocated_disbursements_minor)::TEXT
FROM amounts
GROUP BY currency
ORDER BY currency;
