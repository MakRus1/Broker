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
)::public.contract_v1,
    {sort_key} AS text_key,
    {date_key} AS date_key
FROM contracts
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
