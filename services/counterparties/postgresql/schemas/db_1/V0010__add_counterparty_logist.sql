ALTER TABLE counterparties ADD COLUMN logist TEXT;

CREATE TYPE public.counterparty_v5 AS (
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
    logist TEXT
);
