SELECT ROW(
    id, name, revision, archived, legal_name, country_code,
    tax_id, registration_id, legal_address, roles, contacts, kpp
)::public.counterparty_v4
FROM counterparties
WHERE organization_id = $1::UUID
  AND ($2::UUID IS NULL OR id > $2::UUID)
  AND ($4 = '' OR strpos(lower(concat_ws(' ', name, legal_name, tax_id)), lower($4)) > 0)
  AND ($5::TEXT IS NULL OR $5 = ANY(roles))
  AND ($6 = 'all' OR archived = ($6 = 'archived'))
ORDER BY id
LIMIT $3;
