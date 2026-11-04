SELECT
    COALESCE(jsonb_object_agg(locale, messages) FILTER (WHERE scope = $1), '{}'::JSONB),
    COALESCE(jsonb_object_agg(locale, messages) FILTER (WHERE scope = 'global' AND $1 <> 'global'), '{}'::JSONB)
FROM (
    SELECT 'global' AS scope, locale, messages FROM localization_catalogs
    UNION ALL
    SELECT organization_id::TEXT AS scope, locale, messages
    FROM organization_localization_catalogs
    WHERE organization_id::TEXT = $1
) AS catalogs;
