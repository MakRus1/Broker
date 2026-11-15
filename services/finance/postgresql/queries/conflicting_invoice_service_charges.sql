-- $1 — организация.
-- $2 — идентификаторы проверяемых начислений.
-- $3 — идентификатор счёта, который исключается из проверки.
SELECT service_charge_id
FROM invoice_service_charges
WHERE organization_id = $1::UUID
    AND service_charge_id = ANY($2::UUID[])
    AND invoice_id <> $3::UUID
    AND NOT released;
