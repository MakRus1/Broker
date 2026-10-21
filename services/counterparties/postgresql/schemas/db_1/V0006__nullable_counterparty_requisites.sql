-- Отсутствующие реквизиты хранятся как NULL; старые типы сохраняются.
ALTER TABLE counterparties
    ALTER COLUMN legal_name DROP NOT NULL,
    ALTER COLUMN legal_name DROP DEFAULT,
    ALTER COLUMN country_code DROP NOT NULL,
    ALTER COLUMN country_code DROP DEFAULT,
    ALTER COLUMN tax_id DROP NOT NULL,
    ALTER COLUMN tax_id DROP DEFAULT,
    ALTER COLUMN registration_id DROP NOT NULL,
    ALTER COLUMN registration_id DROP DEFAULT,
    ALTER COLUMN legal_address DROP NOT NULL,
    ALTER COLUMN legal_address DROP DEFAULT;

UPDATE counterparties
SET legal_name = NULLIF(legal_name, ''),
    country_code = NULLIF(country_code, ''),
    tax_id = NULLIF(tax_id, ''),
    registration_id = NULLIF(registration_id, ''),
    legal_address = NULLIF(legal_address, '');

UPDATE counterparties
SET contacts = (
    SELECT COALESCE(jsonb_agg(contact || jsonb_build_object(
        'position', NULLIF(contact->>'position', ''),
        'email', NULLIF(contact->>'email', ''),
        'phone', NULLIF(contact->>'phone', '')
    )), '[]'::JSONB)
    FROM jsonb_array_elements(contacts) AS contact
);

CREATE TYPE public.counterparty_v3 AS (
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
