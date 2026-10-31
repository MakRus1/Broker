SELECT reporting_currency, revision, outgoing_number_template, incoming_number_template, reset_numbers_yearly
FROM finance_settings
WHERE organization_id = $1::UUID;
