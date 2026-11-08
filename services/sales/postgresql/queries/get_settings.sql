SELECT ROW(revision, stages, inquiry_number_template, reset_number_yearly)::public.sales_settings_v1
FROM sales_settings
WHERE organization_id = $1::UUID;
