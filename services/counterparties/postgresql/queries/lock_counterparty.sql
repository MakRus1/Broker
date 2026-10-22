SELECT ROW(
    id, name, revision, archived, legal_name, country_code,
    tax_id, registration_id, legal_address, roles, contacts, kpp
)::public.counterparty_v4
FROM counterparties
WHERE organization_id = $1::UUID AND id = $2::UUID
FOR UPDATE;
