-- Операция одновременно является долговечной записью очереди доставки.
CREATE TABLE creation_operations (
    tenant_id UUID NOT NULL,
    id UUID NOT NULL DEFAULT gen_random_uuid(),
    item_id UUID NOT NULL,
    quantity INTEGER NOT NULL CHECK (quantity BETWEEN 1 AND 100),
    status TEXT NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'succeeded', 'rejected')),
    payload JSONB NOT NULL,
    result JSONB,
    attempts INTEGER NOT NULL DEFAULT 0,
    next_attempt_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    lease_until TIMESTAMPTZ,
    lease_token UUID,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (tenant_id, id),
    FOREIGN KEY (tenant_id, item_id) REFERENCES shipment_items (tenant_id, id)
);

CREATE INDEX creation_operations_item_idx ON creation_operations (tenant_id, item_id, status);
CREATE INDEX creation_operations_due_idx ON creation_operations (next_attempt_at) WHERE status = 'pending';
