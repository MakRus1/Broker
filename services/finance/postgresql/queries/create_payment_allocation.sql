-- $1 — организация.
-- $2 — идентификатор платежа.
-- $3 — идентификатор счёта.
-- $4 — сумма распределения в минимальных единицах валюты.
INSERT INTO payment_allocations (organization_id, payment_id, invoice_id, amount_minor)
VALUES ($1::UUID, $2::UUID, $3::UUID, $4::BIGINT);
