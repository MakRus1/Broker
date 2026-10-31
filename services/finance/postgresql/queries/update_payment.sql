UPDATE payments AS payment
SET
    kind = value.kind,
    number = value.number,
    payment_date = value.payment_date,
    own_company_id = value.own_company_id,
    own_company_name = value.own_company_name,
    counterparty_id = value.counterparty_id,
    counterparty_name = value.counterparty_name,
    own_bank_account_id = value.own_bank_account_id,
    counterparty_bank_account_id = value.counterparty_bank_account_id,
    currency = value.currency,
    amount_minor = value.amount_minor,
    purpose = value.purpose,
    note = value.note,
    archived = value.archived,
    revision = value.revision
FROM (SELECT ($2::public.payment_v1).*) AS value
WHERE payment.organization_id = $1::UUID
    AND payment.id = value.id
    AND payment.revision = $3::BIGINT
RETURNING payment.id;
