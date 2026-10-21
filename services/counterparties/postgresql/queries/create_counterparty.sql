INSERT INTO counterparties (
    organization_id, id, name, legal_name, country_code, tax_id,
    registration_id, legal_address, roles, contacts
)
VALUES ($1::UUID, $2::UUID, $3, $4, $5, $6, $7, $8, $9::TEXT[], $10::JSONB)
ON CONFLICT (organization_id, id) DO NOTHING
RETURNING ROW(
    id, name, revision, archived, legal_name, country_code,
    tax_id, registration_id, legal_address, roles, contacts
)::public.counterparty_v2;
