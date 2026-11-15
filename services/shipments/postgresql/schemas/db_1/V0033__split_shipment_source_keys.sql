ALTER TABLE shipment_sources DROP CONSTRAINT shipment_sources_pkey;
ALTER TABLE shipment_sources RENAME COLUMN source_id TO manual_idempotency_key;
ALTER TABLE shipment_sources
    ALTER COLUMN manual_idempotency_key DROP NOT NULL,
    ADD COLUMN source_id UUID;

UPDATE shipment_sources
SET source_id = manual_idempotency_key::UUID,
    manual_idempotency_key = NULL
WHERE (producer, source_type) <> ('user', 'manual');

ALTER TABLE shipment_sources
    ADD CONSTRAINT shipment_sources_exactly_one_key
    CHECK (num_nonnulls(source_id, manual_idempotency_key) = 1);

CREATE UNIQUE INDEX shipment_sources_source_id_unique
    ON shipment_sources (organization_id, producer, source_type, source_id)
    WHERE source_id IS NOT NULL;

CREATE UNIQUE INDEX shipment_sources_manual_key_unique
    ON shipment_sources (organization_id, producer, source_type, manual_idempotency_key)
    WHERE manual_idempotency_key IS NOT NULL;
