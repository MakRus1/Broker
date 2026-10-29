ALTER TABLE counterparties ADD COLUMN own_company BOOLEAN NOT NULL DEFAULT FALSE;

CREATE TYPE public.counterparty_v6 AS (
    id UUID,
    name TEXT,
    revision BIGINT,
    archived BOOLEAN,
    legal_name TEXT,
    country_code TEXT,
    tax_id TEXT,
    registration_id TEXT,
    legal_address TEXT,
    roles TEXT[],
    contacts JSONB,
    kpp TEXT,
    logist TEXT,
    own_company BOOLEAN
);

CREATE TABLE bank_accounts (
    organization_id UUID NOT NULL,
    id UUID NOT NULL,
    counterparty_id UUID NOT NULL,
    name TEXT NOT NULL,
    number TEXT NOT NULL,
    currency TEXT NOT NULL,
    bank_name TEXT NOT NULL,
    bic TEXT,
    swift TEXT,
    correspondent_account TEXT,
    iban TEXT,
    is_primary BOOLEAN NOT NULL,
    archived BOOLEAN NOT NULL,
    revision BIGINT NOT NULL,
    PRIMARY KEY (organization_id, id),
    FOREIGN KEY (organization_id, counterparty_id)
        REFERENCES counterparties (organization_id, id)
);

CREATE INDEX bank_accounts_owner_idx ON bank_accounts (organization_id, counterparty_id);
CREATE UNIQUE INDEX bank_accounts_primary_idx
    ON bank_accounts (organization_id, counterparty_id, currency)
    WHERE is_primary AND NOT archived;
CREATE UNIQUE INDEX bank_accounts_number_idx
    ON bank_accounts (organization_id, counterparty_id, currency, number)
    WHERE NOT archived;

CREATE TYPE public.bank_account_v1 AS (
    id UUID,
    counterparty_id UUID,
    name TEXT,
    number TEXT,
    currency TEXT,
    bank_name TEXT,
    bic TEXT,
    swift TEXT,
    correspondent_account TEXT,
    iban TEXT,
    is_primary BOOLEAN,
    archived BOOLEAN,
    revision BIGINT
);
