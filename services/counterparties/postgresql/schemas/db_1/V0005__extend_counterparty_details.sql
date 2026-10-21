-- Реквизиты дополняются без изменения существующих идентификаторов и старого типа.
ALTER TABLE counterparties
    ADD COLUMN revision BIGINT NOT NULL DEFAULT 1 CHECK (revision > 0),
    ADD COLUMN archived BOOLEAN NOT NULL DEFAULT FALSE,
    ADD COLUMN legal_name TEXT NOT NULL DEFAULT '',
    ADD COLUMN country_code TEXT NOT NULL DEFAULT '' CHECK (country_code ~ '^(|[A-Z]{2})$'),
    ADD COLUMN tax_id TEXT NOT NULL DEFAULT '',
    ADD COLUMN registration_id TEXT NOT NULL DEFAULT '',
    ADD COLUMN legal_address TEXT NOT NULL DEFAULT '',
    ADD COLUMN roles TEXT[] NOT NULL DEFAULT '{}',
    ADD COLUMN contacts JSONB NOT NULL DEFAULT '[]'::JSONB CHECK (jsonb_typeof(contacts) = 'array');

CREATE UNIQUE INDEX counterparties_requisites_idx
    ON counterparties (organization_id, country_code, tax_id)
    WHERE tax_id <> '';

CREATE TYPE public.counterparty_v2 AS (
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
    contacts JSONB
);
