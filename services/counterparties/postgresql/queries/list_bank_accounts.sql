SELECT ROW(
    id,
    counterparty_id,
    name,
    number,
    currency,
    bank_name,
    bic,
    swift,
    correspondent_account,
    iban,
    is_primary,
    archived,
    revision
)::public.bank_account_v1
FROM bank_accounts
WHERE organization_id = $1::UUID AND counterparty_id = $2::UUID
ORDER BY archived, currency, is_primary DESC, name, id;
