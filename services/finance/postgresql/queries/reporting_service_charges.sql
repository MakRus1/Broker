SELECT service_date, currency, invoice_currency, total_minor, invoice_total_minor,
    direction, transportation_id
FROM service_charges
WHERE organization_id = $1::UUID
    AND shipment_id = $2::UUID
    AND ($3::UUID IS NULL OR transportation_id = $3::UUID)
    AND NOT archived;
