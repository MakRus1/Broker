UPDATE contracts AS stored
SET
    own_company_id = value.own_company_id,
    counterparty_id = value.counterparty_id,
    number = value.number,
    contract_date = value.contract_date,
    valid_until = value.valid_until,
    currency = value.currency,
    own_bank_account_id = value.own_bank_account_id,
    counterparty_bank_account_id = value.counterparty_bank_account_id,
    payment_delay_days = value.payment_delay_days,
    note = value.note,
    archived = value.archived,
    revision = value.revision
FROM (SELECT ($2::public.contract_v1).*) AS value
WHERE stored.organization_id = $1::UUID
    AND stored.id = value.id
    AND stored.revision = $3::BIGINT
RETURNING ROW(
    stored.id,
    stored.own_company_id,
    stored.counterparty_id,
    stored.number,
    stored.contract_date,
    stored.valid_until,
    stored.currency,
    stored.own_bank_account_id,
    stored.counterparty_bank_account_id,
    stored.payment_delay_days,
    stored.note,
    stored.archived,
    stored.revision
)::public.contract_v1;
