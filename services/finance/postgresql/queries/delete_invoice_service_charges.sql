-- NULL в третьем параметре: заменить весь состав; UUID: исключить одну услугу.
DELETE FROM invoice_service_charges
WHERE organization_id = $1::UUID
    AND invoice_id = $2::UUID
    AND ($3::UUID IS NULL OR service_charge_id = $3::UUID);
