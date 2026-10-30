INSERT INTO contracts (
    organization_id,
    id,
    own_company_id,
    counterparty_id,
    number,
    contract_date,
    valid_until,
    currency,
    own_bank_account_id,
    counterparty_bank_account_id,
    payment_delay_days,
    note,
    archived,
    revision
)
SELECT
    $1::UUID,
    value.*
FROM (SELECT ($2::public.contract_v1).*) AS value
ON CONFLICT (organization_id, id) DO NOTHING
RETURNING ROW(
    id,
    own_company_id,
    counterparty_id,
    number,
    contract_date,
    valid_until,
    currency,
    own_bank_account_id,
    counterparty_bank_account_id,
    payment_delay_days,
    note,
    archived,
    revision
)::public.contract_v1;
