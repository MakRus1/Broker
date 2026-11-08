SELECT EXISTS (
    SELECT 1
    FROM sales_history
    WHERE organization_id = $1::UUID AND entity = 'inquiries'
        AND id = $2::UUID AND revision = $3
        AND COALESCE(snapshot->>'cargo_description', '') !~ '[^[:space:]]'
        AND (jsonb_strip_nulls(snapshot) - 'revision' - 'cargo_description')
            = (jsonb_strip_nulls($4::JSONB) - 'revision' - 'cargo_description')
);
