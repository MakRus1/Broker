CREATE INDEX invoices_counterparty_settlements
    ON invoices (organization_id, counterparty_id, own_company_id, currency)
    WHERE NOT archived;

CREATE INDEX payments_counterparty_settlements
    ON payments (organization_id, counterparty_id, own_company_id, currency)
    WHERE NOT archived;
