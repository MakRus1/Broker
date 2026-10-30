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
)::public.payment_v1, payment.amount_minor - COALESCE(allocated.amount_minor, 0)::BIGINT
FROM payments AS payment
LEFT JOIN LATERAL (
    SELECT SUM(amount_minor) AS amount_minor
    FROM payment_allocations AS allocation
    WHERE allocation.organization_id = payment.organization_id AND allocation.payment_id = payment.id
) AS allocated ON TRUE
WHERE payment.organization_id = $1::UUID AND payment.kind = $2::TEXT
    AND payment.own_company_id = $3::UUID AND payment.counterparty_id = $4::UUID
    AND payment.currency = $5::TEXT AND NOT payment.archived
    AND payment.amount_minor > COALESCE(allocated.amount_minor, 0)
    AND ($6::TEXT IS NULL OR strpos(lower(payment.number), lower($6::TEXT)) > 0)
    AND ($7::UUID IS NULL OR payment.id > $7::UUID)
ORDER BY payment.id
LIMIT $8::INTEGER;
