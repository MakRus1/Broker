CREATE TYPE public.payment_v1 AS (
    id UUID,
    kind TEXT,
    number TEXT,
    payment_date DATE,
    own_company_id UUID,
    own_company_name TEXT,
    counterparty_id UUID,
    counterparty_name TEXT,
    own_bank_account_id UUID,
    counterparty_bank_account_id UUID,
    currency TEXT,
    amount_minor BIGINT,
    purpose TEXT,
    note TEXT,
    archived BOOLEAN,
    revision BIGINT
);

CREATE TABLE payments (
    organization_id UUID NOT NULL,
    id UUID NOT NULL,
    kind TEXT NOT NULL,
    number TEXT NOT NULL,
    payment_date DATE NOT NULL,
    own_company_id UUID NOT NULL,
    own_company_name TEXT NOT NULL,
    counterparty_id UUID NOT NULL,
    counterparty_name TEXT NOT NULL,
    own_bank_account_id UUID,
    counterparty_bank_account_id UUID,
    currency TEXT NOT NULL,
    amount_minor BIGINT NOT NULL,
    purpose TEXT,
    note TEXT,
    archived BOOLEAN NOT NULL,
    revision BIGINT NOT NULL,
    PRIMARY KEY (organization_id, id)
);

CREATE INDEX payments_registry ON payments (organization_id, kind, payment_date, id);

CREATE TABLE payment_allocations (
    organization_id UUID NOT NULL,
    payment_id UUID NOT NULL,
    invoice_id UUID NOT NULL,
    amount_minor BIGINT NOT NULL,
    PRIMARY KEY (organization_id, payment_id, invoice_id),
    FOREIGN KEY (organization_id, payment_id) REFERENCES payments (organization_id, id),
    FOREIGN KEY (organization_id, invoice_id) REFERENCES invoices (organization_id, id)
);
CREATE INDEX payment_allocations_invoice ON payment_allocations (organization_id, invoice_id);

CREATE TABLE payment_history (
    organization_id UUID NOT NULL,
    payment_id UUID NOT NULL,
    revision BIGINT NOT NULL,
    actor TEXT NOT NULL,
    changed_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    snapshot JSONB NOT NULL,
    PRIMARY KEY (organization_id, payment_id, revision)
);
