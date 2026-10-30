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
)::public.payment_v1, allocation.amount_minor
FROM payments AS payment
JOIN payment_allocations AS allocation
    ON allocation.organization_id = payment.organization_id AND allocation.payment_id = payment.id
WHERE allocation.organization_id = $1::UUID AND allocation.invoice_id = $2::UUID
    AND NOT payment.archived
ORDER BY payment.payment_date, payment.id;
