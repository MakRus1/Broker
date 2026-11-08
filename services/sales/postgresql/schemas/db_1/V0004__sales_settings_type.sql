CREATE TYPE public.sales_settings_v1 AS (
    revision BIGINT,
    stages JSONB,
    inquiry_number_template TEXT,
    reset_number_yearly BOOLEAN
);

-- Снимки старых редакций дополняются организацией из журнала.
UPDATE sales_history
SET snapshot = snapshot || jsonb_build_object('organization_id', organization_id)
WHERE NOT snapshot ? 'organization_id';
