-- $1 — организация, $2 — контрагент, $3 — своё юрлицо, $4 — договор,
-- $5 — валюта, $6/$7 — включительные границы периода.
WITH selected_invoices AS (
    SELECT id, direction, number, invoice_date, own_company_name, currency, total_minor
    FROM invoices
    WHERE organization_id = $1::UUID
        AND counterparty_id = $2::UUID
        AND ($3::UUID IS NULL OR own_company_id = $3::UUID)
        AND ($4::UUID IS NULL OR contract_id = $4::UUID)
        AND ($5::TEXT IS NULL OR currency = $5::TEXT)
        AND invoice_date <= $7::DATE
        AND NOT archived
), selected_payments AS (
    SELECT
        payment.id, payment.kind, payment.number, payment.payment_date,
        payment.own_company_name, payment.currency,
        CASE WHEN $4::UUID IS NULL THEN payment.amount_minor
            ELSE COALESCE((
                SELECT SUM(allocation.amount_minor)
                FROM payment_allocations AS allocation
                JOIN invoices AS invoice
                    ON invoice.organization_id = allocation.organization_id
                    AND invoice.id = allocation.invoice_id
                WHERE allocation.organization_id = payment.organization_id
                    AND allocation.payment_id = payment.id
                    AND invoice.contract_id = $4::UUID
                    AND NOT invoice.archived
            ), 0)
        END AS amount_minor
    FROM payments AS payment
    WHERE payment.organization_id = $1::UUID
        AND payment.counterparty_id = $2::UUID
        AND ($3::UUID IS NULL OR payment.own_company_id = $3::UUID)
        AND ($5::TEXT IS NULL OR payment.currency = $5::TEXT)
        AND payment.payment_date <= $7::DATE
        AND NOT payment.archived
), operations AS (
    SELECT
        id, direction || '_invoice' AS kind, number, invoice_date AS document_date,
        own_company_name, currency,
        CASE WHEN direction = 'outgoing' THEN total_minor ELSE 0 END AS debit_minor,
        CASE WHEN direction = 'incoming' THEN total_minor ELSE 0 END AS credit_minor
    FROM selected_invoices

    UNION ALL

    SELECT
        id, kind, number, payment_date, own_company_name, currency,
        CASE WHEN kind = 'disbursement' THEN amount_minor ELSE 0 END,
        CASE WHEN kind = 'receipt' THEN amount_minor ELSE 0 END
    FROM selected_payments
    WHERE amount_minor > 0
), totals AS (
    SELECT
        currency,
        SUM(CASE WHEN document_date < $6::DATE
            THEN debit_minor - credit_minor ELSE 0 END) AS opening_minor,
        SUM(CASE WHEN document_date >= $6::DATE THEN debit_minor ELSE 0 END) AS debit_minor,
        SUM(CASE WHEN document_date >= $6::DATE THEN credit_minor ELSE 0 END) AS credit_minor,
        SUM(debit_minor - credit_minor) AS closing_minor
    FROM operations
    GROUP BY currency
), period_operations AS (
    SELECT
        operation.*,
        total.opening_minor + SUM(operation.debit_minor - operation.credit_minor) OVER (
            PARTITION BY operation.currency
            ORDER BY operation.document_date, operation.kind, operation.id
            ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
        ) AS balance_minor
    FROM operations AS operation
    JOIN totals AS total ON total.currency = operation.currency
    WHERE operation.document_date >= $6::DATE
    ORDER BY operation.document_date, operation.kind, operation.id
    LIMIT 10001
)
SELECT
    TRUE AS is_total, currency, NULL::UUID AS document_id, NULL::DATE AS document_date,
    NULL::TEXT AS kind, ''::TEXT AS number, ''::TEXT AS own_company_name,
    debit_minor::TEXT, credit_minor::TEXT, closing_minor::TEXT AS balance_minor,
    opening_minor::TEXT
FROM totals

UNION ALL

SELECT
    FALSE, currency, id, document_date, kind, number, own_company_name,
    debit_minor::TEXT, credit_minor::TEXT, balance_minor::TEXT, '0'::TEXT
FROM period_operations
ORDER BY is_total DESC, document_date, kind, document_id, currency;
