CREATE TABLE service_charge_imports (
    organization_id UUID NOT NULL,
    producer TEXT NOT NULL,
    id UUID NOT NULL,
    document JSONB NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (organization_id, producer, id)
);
