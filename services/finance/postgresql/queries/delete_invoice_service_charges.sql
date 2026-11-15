-- $1 — организация.
-- $2 — идентификатор счёта.
-- $3 — идентификатор начисления (NULL удаляет весь состав счёта).
DELETE FROM invoice_service_charges
WHERE organization_id = $1::UUID
    AND invoice_id = $2::UUID
    AND ($3::UUID IS NULL OR service_charge_id = $3::UUID);
