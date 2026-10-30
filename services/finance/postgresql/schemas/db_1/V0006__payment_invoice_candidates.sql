-- Выбор счетов по сторонам и валюте с продолжением по id.
CREATE INDEX invoices_payment_candidates
    ON invoices (organization_id, direction, own_company_id, counterparty_id, currency, id)
    WHERE NOT archived;

