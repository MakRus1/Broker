INSERT INTO counterparties (
    organization_id, id, name, legal_name, country_code, tax_id,
    registration_id, legal_address, roles, contacts, kpp, logist
)
SELECT $1::UUID, value.id, value.name, value.legal_name, value.country_code,
       value.tax_id, value.registration_id, value.legal_address, value.roles, value.contacts, value.kpp, value.logist
FROM (SELECT ($2::public.counterparty_v5).*) AS value
ON CONFLICT (organization_id, id) DO NOTHING
RETURNING ROW(
    id, name, revision, archived, legal_name, country_code,
    tax_id, registration_id, legal_address, roles, contacts, kpp, logist
)::public.counterparty_v5;
