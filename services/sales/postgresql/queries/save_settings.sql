INSERT INTO sales_settings (organization_id, revision, stages, inquiry_number_template, reset_number_yearly)
VALUES ($1::UUID, 1, $3::JSONB, $4::TEXT, $5::BOOLEAN)
ON CONFLICT (organization_id) DO UPDATE
SET revision = sales_settings.revision + 1, stages = EXCLUDED.stages,
    inquiry_number_template = EXCLUDED.inquiry_number_template,
    reset_number_yearly = EXCLUDED.reset_number_yearly
WHERE sales_settings.revision = $2
    AND sales_settings.revision < 9223372036854775807
RETURNING revision;
