WITH updated AS (
    UPDATE contract_numbering
    SET number_template = $2::TEXT, reset_number_yearly = $3::BOOLEAN, revision = revision + 1
    WHERE organization_id = $1::UUID AND revision = $4::BIGINT
    RETURNING number_template, reset_number_yearly, revision
), inserted AS (
    INSERT INTO contract_numbering (organization_id, number_template, reset_number_yearly, revision)
    SELECT $1::UUID, $2::TEXT, $3::BOOLEAN, 1 WHERE $4::BIGINT = 0
    ON CONFLICT (organization_id) DO NOTHING
    RETURNING number_template, reset_number_yearly, revision
)
SELECT * FROM updated UNION ALL SELECT * FROM inserted;
