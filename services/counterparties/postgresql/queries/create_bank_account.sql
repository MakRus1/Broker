INSERT INTO bank_accounts (
    organization_id,
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
)
SELECT
    $1::UUID,
    value.*
FROM (SELECT ($2::public.bank_account_v1).*) AS value
ON CONFLICT (organization_id, id) DO NOTHING
RETURNING ROW(
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
)::public.bank_account_v1;
