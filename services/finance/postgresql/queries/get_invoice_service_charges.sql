-- $1 — организация.
-- $2 — идентификатор счёта.
SELECT snapshot FROM invoice_service_charges
WHERE organization_id = $1::UUID AND invoice_id = $2::UUID
ORDER BY service_charge_id;
