UPDATE counterparties
SET name = $3, legal_name = $4, country_code = $5, tax_id = $6,
    registration_id = $7, legal_address = $8, roles = $9::TEXT[],
    contacts = $10::JSONB, revision = revision + 1
WHERE organization_id = $1::UUID AND id = $2::UUID
RETURNING ROW(
    id, name, revision, archived, legal_name, country_code,
    tax_id, registration_id, legal_address, roles, contacts
)::public.counterparty_v3;
