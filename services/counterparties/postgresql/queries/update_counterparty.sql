UPDATE counterparties AS stored
SET name = value.name,
    legal_name = value.legal_name,
    country_code = value.country_code,
    tax_id = value.tax_id,
    registration_id = value.registration_id,
    legal_address = value.legal_address,
    roles = value.roles,
    contacts = value.contacts,
    kpp = value.kpp,
    revision = stored.revision + 1
FROM (SELECT ($2::public.counterparty_v4).*) AS value
WHERE stored.organization_id = $1::UUID AND stored.id = value.id
RETURNING ROW(
    stored.id, stored.name, stored.revision, stored.archived, stored.legal_name, stored.country_code,
    stored.tax_id, stored.registration_id, stored.legal_address, stored.roles, stored.contacts, stored.kpp
)::public.counterparty_v4;
