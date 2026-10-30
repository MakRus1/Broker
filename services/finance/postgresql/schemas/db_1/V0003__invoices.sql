CREATE TYPE public.invoice_v1 AS (
    id UUID,
    direction TEXT,
    number TEXT,
    invoice_date DATE,
    due_date DATE,
    own_company_id UUID,
    own_company_name TEXT,
    counterparty_id UUID,
    counterparty_name TEXT,
    own_bank_account_id UUID,
    counterparty_bank_account_id UUID,
    contract_id UUID,
    contract_number TEXT,
    currency TEXT,
    note TEXT,
    archived BOOLEAN,
    revision BIGINT,
    net_minor BIGINT,
    vat_minor BIGINT,
    total_minor BIGINT
);

CREATE TABLE invoices (
    organization_id UUID NOT NULL,
    id UUID NOT NULL,
    direction TEXT NOT NULL,
    number TEXT NOT NULL,
    invoice_date DATE NOT NULL,
    due_date DATE,
    own_company_id UUID NOT NULL,
    own_company_name TEXT NOT NULL,
    counterparty_id UUID NOT NULL,
    counterparty_name TEXT NOT NULL,
    own_bank_account_id UUID,
    counterparty_bank_account_id UUID,
    contract_id UUID,
    contract_number TEXT,
    currency TEXT NOT NULL,
    note TEXT,
    archived BOOLEAN NOT NULL,
    revision BIGINT NOT NULL,
    net_minor BIGINT NOT NULL,
    vat_minor BIGINT NOT NULL,
    total_minor BIGINT NOT NULL,
    PRIMARY KEY (organization_id, id)
);

CREATE INDEX invoices_registry ON invoices (organization_id, direction, invoice_date, id);

CREATE TABLE invoice_entries (
    organization_id UUID NOT NULL,
    invoice_id UUID NOT NULL,
    entry_id UUID NOT NULL,
    released BOOLEAN NOT NULL,
    snapshot public.financial_entry_v1 NOT NULL,
    PRIMARY KEY (organization_id, invoice_id, entry_id),
    FOREIGN KEY (organization_id, invoice_id) REFERENCES invoices (organization_id, id),
    FOREIGN KEY (organization_id, entry_id) REFERENCES financial_entries (organization_id, id)
);

CREATE UNIQUE INDEX invoice_entries_active ON invoice_entries (organization_id, entry_id)
    WHERE NOT released;

CREATE TABLE invoice_history (
    organization_id UUID NOT NULL,
    invoice_id UUID NOT NULL,
    revision BIGINT NOT NULL,
    actor TEXT NOT NULL,
    occurred_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    snapshot JSONB NOT NULL,
    PRIMARY KEY (organization_id, invoice_id, revision),
    FOREIGN KEY (organization_id, invoice_id) REFERENCES invoices (organization_id, id)
);

CREATE INDEX financial_entries_invoice_candidates
    ON financial_entries (organization_id, counterparty_id, currency, contract_id, direction, id)
    WHERE NOT archived;

CREATE COLLATION IF NOT EXISTS public.registry_number_order (
    provider = icu,
    locale = 'und-u-kn-true',
    deterministic = true
);
