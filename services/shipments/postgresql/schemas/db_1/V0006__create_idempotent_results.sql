CREATE TABLE idempotent_results (
    tenant_id UUID NOT NULL,
    action TEXT NOT NULL,
    key TEXT NOT NULL,
    request JSONB NOT NULL,
    result JSONB NOT NULL,
    PRIMARY KEY (tenant_id, action, key)
);
