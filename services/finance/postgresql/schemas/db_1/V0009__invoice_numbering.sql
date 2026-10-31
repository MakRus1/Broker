CREATE TABLE document_number_counters (
    organization_id UUID NOT NULL,
    kind TEXT NOT NULL,
    period INTEGER NOT NULL,
    value BIGINT NOT NULL,
    PRIMARY KEY (organization_id, kind, period)
);

ALTER TABLE finance_settings ADD COLUMN outgoing_number_template TEXT;
ALTER TABLE finance_settings ADD COLUMN incoming_number_template TEXT;
ALTER TABLE finance_settings ADD COLUMN reset_numbers_yearly BOOLEAN;
UPDATE finance_settings SET outgoing_number_template = 'ИС-{sequence}/{yyyy}',
    incoming_number_template = 'ВС-{sequence}/{yyyy}', reset_numbers_yearly = false;
ALTER TABLE finance_settings ALTER COLUMN outgoing_number_template SET NOT NULL;
ALTER TABLE finance_settings ALTER COLUMN incoming_number_template SET NOT NULL;
ALTER TABLE finance_settings ALTER COLUMN reset_numbers_yearly SET NOT NULL;
