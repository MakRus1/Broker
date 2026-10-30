CREATE TABLE contracts (
    organization_id UUID NOT NULL,
    id UUID NOT NULL,
    own_company_id UUID NOT NULL,
    counterparty_id UUID NOT NULL,
    number TEXT NOT NULL,
    contract_date DATE NOT NULL,
    valid_until DATE,
    currency TEXT NOT NULL,
    own_bank_account_id UUID,
    counterparty_bank_account_id UUID,
    payment_delay_days INTEGER,
    note TEXT,
    archived BOOLEAN NOT NULL,
    revision BIGINT NOT NULL,
    PRIMARY KEY (organization_id, id)
);

CREATE INDEX contracts_own_company_idx ON contracts (organization_id, own_company_id);
CREATE INDEX contracts_counterparty_idx ON contracts (organization_id, counterparty_id);

CREATE TABLE contract_history (
    organization_id UUID NOT NULL,
    contract_id UUID NOT NULL,
    revision BIGINT NOT NULL,
    actor TEXT NOT NULL,
    occurred_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    snapshot JSONB NOT NULL,
    PRIMARY KEY (organization_id, contract_id, revision),
    FOREIGN KEY (organization_id, contract_id) REFERENCES contracts (organization_id, id)
);

CREATE TYPE public.contract_v1 AS (
    id UUID,
    own_company_id UUID,
    counterparty_id UUID,
    number TEXT,
    contract_date DATE,
    valid_until DATE,
    currency TEXT,
    own_bank_account_id UUID,
    counterparty_bank_account_id UUID,
    payment_delay_days INTEGER,
    note TEXT,
    archived BOOLEAN,
    revision BIGINT
);

-- Числовые части номера сравниваются как числа: S-2 раньше S-10.
-- Регистр по-прежнему нормализуется в запросе; одинаковые ключи разделяет id.
CREATE COLLATION public.registry_number_order (
    provider = icu,
    locale = 'und-u-kn-true',
    deterministic = true
);

CREATE INDEX contracts_number_idx ON contracts (organization_id, (lower(number) COLLATE public.registry_number_order), id);
CREATE INDEX contracts_date_idx ON contracts (organization_id, contract_date, id);
CREATE INDEX contracts_valid_until_idx ON contracts (organization_id, (COALESCE(valid_until, DATE '9999-12-31')), id);
