SELECT ROW(
    id,
    kind,
    number,
    payment_date,
    own_company_id,
    own_company_name,
    counterparty_id,
    counterparty_name,
    own_bank_account_id,
    counterparty_bank_account_id,
    currency,
    amount_minor,
    purpose,
    note,
    archived,
    revision
)::public.payment_v1,
    {sort_key} AS text_key,
    {date_key} AS date_key
FROM payments AS invoice
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};

