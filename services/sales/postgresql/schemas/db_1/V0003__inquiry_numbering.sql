ALTER TABLE sales_settings
    ADD COLUMN inquiry_number_template TEXT NOT NULL DEFAULT 'ENQ-{sequence}',
    ADD COLUMN reset_number_yearly BOOLEAN NOT NULL DEFAULT FALSE;

CREATE TABLE document_number_counters (
    organization_id UUID NOT NULL,
    kind TEXT NOT NULL,
    period INTEGER NOT NULL,
    value BIGINT NOT NULL CHECK (value > 0),
    PRIMARY KEY (organization_id, kind, period)
);

INSERT INTO document_number_counters (organization_id, kind, period, value)
SELECT organization_id, 'inquiry', 0, MAX(number)
FROM sales_inquiries
GROUP BY organization_id;

ALTER TABLE sales_inquiries ALTER COLUMN number DROP IDENTITY;
ALTER TABLE sales_inquiries ALTER COLUMN number TYPE TEXT USING 'ENQ-' || number::TEXT;

CREATE COLLATION IF NOT EXISTS public.registry_number_order
    (provider = icu, locale = 'und-u-kn-true', deterministic = true);

CREATE INDEX sales_inquiries_number_idx ON sales_inquiries
    (organization_id, (lower(number) COLLATE public.registry_number_order), id);
