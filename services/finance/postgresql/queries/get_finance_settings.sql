SELECT reporting_currency, revision FROM finance_settings WHERE organization_id = $1::UUID;
