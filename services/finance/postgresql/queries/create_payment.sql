INSERT INTO payments (
    organization_id,
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
)
SELECT $1::UUID, value.*
FROM (SELECT ($2::public.payment_v1).*) AS value
ON CONFLICT (organization_id, id) DO NOTHING
RETURNING id;
