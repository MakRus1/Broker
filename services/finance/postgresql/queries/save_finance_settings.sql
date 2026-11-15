-- $1 — организация.
-- $2 — код валюты отчётности.
-- $3 — ожидаемая редакция (0 для создания).
-- $4 — шаблон номера исходящего счёта.
-- $5 — шаблон номера входящего счёта.
-- $6 — признак ежегодного сброса счётчиков.
WITH updated AS (
    UPDATE finance_settings
    SET reporting_currency = $2::TEXT, revision = revision + 1,
        outgoing_number_template = $4::TEXT, incoming_number_template = $5::TEXT, reset_numbers_yearly = $6::BOOLEAN
    WHERE organization_id = $1::UUID AND revision = $3::BIGINT
    RETURNING reporting_currency, revision, outgoing_number_template, incoming_number_template, reset_numbers_yearly
), inserted AS (
    INSERT INTO finance_settings (organization_id, reporting_currency, revision, outgoing_number_template, incoming_number_template, reset_numbers_yearly)
    SELECT $1::UUID, $2::TEXT, 1, $4::TEXT, $5::TEXT, $6::BOOLEAN WHERE $3::BIGINT = 0
    ON CONFLICT (organization_id) DO NOTHING
    RETURNING reporting_currency, revision, outgoing_number_template, incoming_number_template, reset_numbers_yearly
)
SELECT * FROM updated UNION ALL SELECT * FROM inserted;
