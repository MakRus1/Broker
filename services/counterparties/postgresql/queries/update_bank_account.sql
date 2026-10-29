UPDATE bank_accounts AS stored
SET name = value.name,
    number = value.number,
    currency = value.currency,
    bank_name = value.bank_name,
    bic = value.bic,
    swift = value.swift,
    correspondent_account = value.correspondent_account,
    iban = value.iban,
    is_primary = value.is_primary,
    archived = value.archived,
    revision = stored.revision + 1
FROM (SELECT ($2::public.bank_account_v1).*) AS value
WHERE stored.organization_id = $1::UUID
    AND stored.id = value.id
    AND stored.counterparty_id = value.counterparty_id
RETURNING ROW(
    stored.id,
    stored.counterparty_id,
    stored.name,
    stored.number,
    stored.currency,
    stored.bank_name,
    stored.bic,
    stored.swift,
    stored.correspondent_account,
    stored.iban,
    stored.is_primary,
    stored.archived,
    stored.revision
)::public.bank_account_v1;
