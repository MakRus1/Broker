SELECT revision, stages, inquiry_number_template, reset_number_yearly
FROM sales_settings
WHERE organization_id = $1::UUID;
