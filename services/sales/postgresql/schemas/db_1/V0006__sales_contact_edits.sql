ALTER TABLE sales_contacts ADD COLUMN revision BIGINT NOT NULL DEFAULT 1;
ALTER TABLE sales_history DROP CONSTRAINT sales_history_entity_check;
ALTER TABLE sales_history ADD CONSTRAINT sales_history_entity_check
    CHECK (entity IN ('cards', 'inquiries', 'contacts'));
INSERT INTO sales_history (organization_id, entity, id, revision, actor, snapshot)
SELECT organization_id, 'contacts', id, revision,
    author_issuer || '#' || author_subject,
    jsonb_strip_nulls(to_jsonb(sales_contacts) - 'organization_id' - 'request')
FROM sales_contacts;
