-- $1 — организация.
-- $2 — идентификатор платежа.
-- $3 — идентификаторы счетов.
-- $4 — суммы распределения в минимальных единицах валюты в том же порядке.
INSERT INTO payment_allocations (organization_id, payment_id, invoice_id, amount_minor)
SELECT $1::UUID, $2::UUID, allocation.invoice_id, allocation.amount_minor
FROM UNNEST($3::UUID[], $4::BIGINT[]) AS allocation(invoice_id, amount_minor);
