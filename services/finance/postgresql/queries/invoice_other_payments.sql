-- $1 — организация.
-- $2 — идентификаторы счетов.
-- $3 — идентификатор платежа, исключаемого из расчёта.
SELECT allocation.invoice_id, SUM(allocation.amount_minor)::BIGINT
FROM payment_allocations AS allocation
JOIN payments AS payment
    ON payment.organization_id = allocation.organization_id AND payment.id = allocation.payment_id
WHERE allocation.organization_id = $1::UUID
    AND allocation.invoice_id = ANY($2::UUID[])
    AND allocation.payment_id <> $3::UUID
    AND NOT payment.archived
GROUP BY allocation.invoice_id;
