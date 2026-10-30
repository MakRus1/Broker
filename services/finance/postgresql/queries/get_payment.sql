SELECT ROW(
    payment.id,
    payment.kind,
    payment.number,
    payment.payment_date,
    payment.own_company_id,
    payment.own_company_name,
    payment.counterparty_id,
    payment.counterparty_name,
    payment.own_bank_account_id,
    payment.counterparty_bank_account_id,
    payment.currency,
    payment.amount_minor,
    payment.purpose,
    payment.note,
    payment.archived,
    payment.revision
)::public.payment_v1
FROM payments AS payment
WHERE organization_id = $1::UUID AND id = $2::UUID;
