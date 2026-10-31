WITH updated AS (
    UPDATE finance_settings
    SET reporting_currency = $2::TEXT, revision = revision + 1
    WHERE organization_id = $1::UUID AND revision = $3::BIGINT
    RETURNING reporting_currency, revision
), inserted AS (
    INSERT INTO finance_settings (organization_id, reporting_currency, revision)
    SELECT $1::UUID, $2::TEXT, 1 WHERE $3::BIGINT = 0
    ON CONFLICT (organization_id) DO NOTHING
    RETURNING reporting_currency, revision
)
SELECT * FROM updated UNION ALL SELECT * FROM inserted;
