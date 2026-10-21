CREATE TABLE shipments (
    tenant_id UUID NOT NULL,
    id UUID NOT NULL DEFAULT gen_random_uuid(),
    revision BIGINT NOT NULL DEFAULT 1 CHECK (revision > 0),
    number BIGINT GENERATED ALWAYS AS IDENTITY,
    customer_id UUID NOT NULL,
    reference TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (tenant_id, id),
    UNIQUE (tenant_id, number)
);
