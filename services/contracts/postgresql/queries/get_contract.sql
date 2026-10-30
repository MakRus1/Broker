SELECT ROW(
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
)::public.contract_v1
FROM contracts
WHERE organization_id = $1::UUID AND id = $2::UUID;
