CREATE INDEX invoices_counterparty_dates
    ON invoices (organization_id, counterparty_id, invoice_date, id)
    WHERE NOT archived;

CREATE INDEX payments_counterparty_dates
    ON payments (organization_id, counterparty_id, payment_date, id)
    WHERE NOT archived;
