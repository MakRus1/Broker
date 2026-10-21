UPDATE counterparties
SET archived = $3, revision = revision + 1
WHERE organization_id = $1::UUID AND id = $2::UUID
RETURNING ROW(
    id, name, revision, archived, legal_name, country_code,
    tax_id, registration_id, legal_address, roles, contacts
)::public.counterparty_v3;
