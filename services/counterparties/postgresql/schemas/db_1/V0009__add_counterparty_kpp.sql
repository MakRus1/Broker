-- Новое поле не меняет существующий тип: старые поды продолжают использовать v3.
ALTER TABLE counterparties ADD COLUMN kpp TEXT;
ALTER TABLE counterparties ADD CONSTRAINT counterparties_kpp_format
    CHECK (kpp IS NULL OR kpp ~ '^[0-9]{9}$');

CREATE TYPE public.counterparty_v4 AS (
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
    kpp TEXT
);
