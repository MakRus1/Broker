-- $1 — организация.
-- $2 — идентификатор счёта.
-- $3 — снимки начислений для включения в счёт.
-- $4 — признак освобождения начислений из-за архивации счёта.
INSERT INTO invoice_service_charges (organization_id, invoice_id, service_charge_id, released, snapshot)
SELECT
    $1::UUID,
    $2::UUID,
    value.id,
    $4::BOOLEAN,
    value::public.service_charge_v1
FROM UNNEST($3::public.service_charge_v1[]) AS value;
