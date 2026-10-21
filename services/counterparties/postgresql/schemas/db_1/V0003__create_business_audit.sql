CREATE TABLE business_audit (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    tenant_id UUID NOT NULL,
    actor TEXT NOT NULL,
    action TEXT NOT NULL,
    entity_id UUID NOT NULL,
    document JSONB NOT NULL,
    occurred_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX business_audit_entity_idx ON business_audit (tenant_id, entity_id, id);
