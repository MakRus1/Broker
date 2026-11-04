SELECT
    COALESCE(
        (SELECT messages FROM localization_catalogs WHERE locale = $2),
        '{}'::JSONB
    ) || COALESCE(
        (
            SELECT messages
            FROM organization_localization_catalogs
            WHERE organization_id = $1::UUID AND locale = $2
        ),
        '{}'::JSONB
    );
